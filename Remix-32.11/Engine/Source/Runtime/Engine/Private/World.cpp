#include "pch.h"
#include "Shared.h"
#include "hde64.h"
#include <tlhelp32.h>

INIT_MODULE(UWorld);

static int AttemptDeriveFromURL()
{
    return 1;
}

static uint32_t DecodeInstruction(const uint8_t* Code, hde64s& Insn)
{
    auto Len = hde64_disasm(Code, &Insn);

    return (Insn.flags & F_ERROR) ? 0 : Len;
}

static void PatchNops(uint8_t* Address, uint32_t Length)
{
    for (uint32_t i = 0; i < Length; i++)
        Patch<uint8_t>(uint64_t(Address + i), 0x90);
}

static std::vector<HANDLE> SuspendOtherThreads()
{
    std::vector<DWORD> ThreadIds;
    auto Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 Entry{ sizeof(Entry) };

    for (auto bFound = Thread32First(Snapshot, &Entry); bFound; bFound = Thread32Next(Snapshot, &Entry))
    {
        if (Entry.th32OwnerProcessID == GetCurrentProcessId() && Entry.th32ThreadID != GetCurrentThreadId())
            ThreadIds.push_back(Entry.th32ThreadID);
    }

    CloseHandle(Snapshot);

    std::vector<HANDLE> Threads;
    Threads.reserve(ThreadIds.size());

    for (auto ThreadId : ThreadIds)
    {
        auto Thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, ThreadId);

        if (!Thread)
            continue;

        if (SuspendThread(Thread) == DWORD(-1))
        {
            CloseHandle(Thread);
            continue;
        }

        CONTEXT Context{};
        Context.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(Thread, &Context);
        Threads.push_back(Thread);
    }

    return Threads;
}

static void PatchAllNetModes(uint64_t AttemptDeriveFromURLAddress)
{
    auto IsMovOrLea = [](const hde64s& Insn)
    {
        if ((Insn.opcode >= 0x88 && Insn.opcode <= 0x8B) || Insn.opcode == 0x8D || (Insn.opcode >= 0xB0 && Insn.opcode <= 0xBF))
            return true;

        return (Insn.opcode == 0xC6 || Insn.opcode == 0xC7) && Insn.modrm_reg == 0;
    };

    auto Text = IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS)(ImageBase + ((PIMAGE_DOS_HEADER)ImageBase)->e_lfanew));
    auto TextStart = (uint8_t*)(ImageBase + Text->VirtualAddress);
    auto TextSize = Text->Misc.VirtualSize;
    auto Threads = SuspendOtherThreads();

    for (uint32_t i = 0; i + 5 < TextSize; i++)
    {
        if (TextStart[i] != 0xE8 && TextStart[i] != 0xE9)
            continue;

        if (uint64_t(TextStart + i + 5 + *(int32_t*)(TextStart + i + 1)) != AttemptDeriveFromURLAddress)
            continue;

        for (uint32_t Back = 1; Back < 0x1000 && Back <= i; Back++)
        {
            auto Compare = TextStart + i - Back;

            if ((*Compare & 0xF8) != 0x48)
                continue;

            hde64s CompareInsn;
            auto CompareLen = DecodeInstruction(Compare, CompareInsn);

            if (!CompareLen || !CompareInsn.rex_w || CompareInsn.modrm_mod != 1 || CompareInsn.modrm_rm == 4 || CompareInsn.disp.disp8 != offsetof(UWorld, NetDriver))
                continue;

            if (CompareInsn.opcode != 0x39 && (CompareInsn.opcode != 0x83 || CompareInsn.modrm_reg != 7 || CompareInsn.imm.imm8 != 0))
                continue;

            auto Branch = Compare + CompareLen;
            hde64s BranchInsn;
            bool bFoundBranch = false;

            for (int x = 0; x < 12; x++)
            {
                auto Len = DecodeInstruction(Branch, BranchInsn);

                if (!Len)
                    break;

                if (BranchInsn.opcode == 0x74 || BranchInsn.opcode == 0x75 || (BranchInsn.opcode == 0x0F && (BranchInsn.opcode2 == 0x84 || BranchInsn.opcode2 == 0x85)))
                {
                    bFoundBranch = true;
                    break;
                }

                bool bFlagNeutral = IsMovOrLea(BranchInsn) || BranchInsn.opcode == 0x63 || BranchInsn.opcode == 0x90;

                if (BranchInsn.opcode == 0x0F)
                {
                    bFlagNeutral = false;

                    for (auto Opcode : { 0x10, 0x11, 0x14, 0x28, 0x29, 0x2A, 0x57, 0x58, 0x59, 0x5A, 0x5C, 0x5E, 0x6E, 0x6F, 0x7E, 0x7F, 0xB6, 0xB7, 0xBE, 0xBF, 0xC6, 0xD6 })
                        bFlagNeutral |= BranchInsn.opcode2 == Opcode;
                }

                if (!bFlagNeutral)
                    break;

                Branch += Len;
            }

            if (!bFoundBranch)
                continue;

            bool bNear = BranchInsn.opcode == 0x0F;
            bool bJumpsIfNetDriver = bNear ? BranchInsn.opcode2 == 0x85 : BranchInsn.opcode == 0x75;
            auto Code = bJumpsIfNetDriver ? Branch + BranchInsn.len : Branch + BranchInsn.len + (bNear ? (int32_t)BranchInsn.imm.imm32 : (int8_t)BranchInsn.imm.imm8);
            bool bDemoNetDriverCheck = false;

            for (int x = 0; x < 4; x++)
            {
                hde64s Insn;
                auto Len = DecodeInstruction(Code, Insn);

                if (!Len)
                    break;

                Code += Len;

                if (Insn.opcode == 0x8B && Insn.rex_w && Insn.modrm_mod != 3)
                {
                    auto Register = Insn.modrm_reg | (Insn.rex_r << 3);
                    bool bTested = false;

                    for (int y = 0; y < 5; y++)
                    {
                        Len = DecodeInstruction(Code, Insn);

                        if (!Len)
                            break;

                        if (!bTested && Insn.opcode == 0x85 && Insn.rex_w && Insn.modrm_mod == 3 && (Insn.modrm_reg | (Insn.rex_r << 3)) == Register && (Insn.modrm_rm | (Insn.rex_b << 3)) == Register)
                            bTested = true;
                        else if (bTested && (Insn.opcode == 0x74 || (Insn.opcode == 0x0F && Insn.opcode2 == 0x84)))
                        {
                            bDemoNetDriverCheck = true;
                            break;
                        }
                        else if (!IsMovOrLea(Insn))
                            break;

                        Code += Len;
                    }

                    break;
                }

                if (!IsMovOrLea(Insn))
                    break;
            }

            if (!bDemoNetDriverCheck)
                continue;

            PatchNops(Compare, CompareLen);

            if (bJumpsIfNetDriver)
                PatchNops(Branch, BranchInsn.len);
            else if (bNear)
                Patch<uint16_t>(uint64_t(Branch), 0xE990);
            else
                Patch<uint8_t>(uint64_t(Branch), 0xEB);

            FlushInstructionCache(GetCurrentProcess(), Compare, Branch + BranchInsn.len - Compare);
            break;
        }
    }

    for (auto Thread : Threads)
    {
        ResumeThread(Thread);
        CloseHandle(Thread);
    }
}

void UWorld__Init()
{
    auto AttemptDeriveFromURLAddress = ImageBase + 0x2196B08;

    Hook(AttemptDeriveFromURLAddress, AttemptDeriveFromURL);

    PatchAllNetModes(AttemptDeriveFromURLAddress);

    Patch<uint8_t>(ImageBase + 0x338C8F8, 0x1);
}
