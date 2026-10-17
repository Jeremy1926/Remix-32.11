#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteAI/Public/Athena/AI/Systems/AthenaAIServicePlayerBots.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/FortAthenaAIBotController.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/Navigation/AthenaNavSystem.h"
#include "FortniteGame/Source/FortniteAI/Public/Athena/AI/FortAthenaAISpawnerDataComponent_CosmeticLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortGameModeAthena.h"

INIT_MODULE(UAthenaAIServicePlayerBots);

static void (*OnPlaylistLoadedOG)(UAthenaAIServicePlayerBots*);
static void (*StartSpawningOG)(UAthenaAIServicePlayerBots*, void*);
static void (*OnAllBotsSpawnedOG)(UAthenaAIServicePlayerBots*);
static void (*UnpauseWarmupFailSafeOG)(UAthenaAIServicePlayerBots*);

static void (*AssignBotTeamOG)(UAthenaAIServicePlayerBots*, AFortAthenaAIBotController*, AFortTeamInfo*, uint8);

static const std::unordered_map<std::string, std::vector<const wchar_t*>> PhoebeNames = {
    { "PhoebeNames_NA", {
        L"0PingSting", L"12Siblings12", L"2xPump4Life", L"_BigBainPlayz_", L"-=Fr0stDevil=-", L"1eyedInvent0r", L"360walkaway", L"42Wow47", L"2sweaty23", L"0mn1v0r378",
        L"PanicButton47", L"1Forest120", L"8BallWasHere", L"4Regionals", L"90sRunner", L"A Sweaty Dog", L"SpectrumKnight13", L"aaaaaaaaalbert", L"4tnite4ever15", L"aaaaaleeex",
        L"ADudeNamedPhil", L"90cranker7", L"AGenuineHuman", L"ActivateFail", L"aClassyGamer94", L"Agoraphone-8", L"AGoodOmen", L"Agua_Cat", L"Ahat4aMouse", L"AggroHamster59",
        L"Aim Like Idaho", L"Albacore6", L"airplanelegs", L"AligatorPear", L"TheBe$t", L"Allsmyles:D", L"AllTheCandy", L"Archibard11", L"amygdallama", L"AmpleBlindspot",
        L"ALLCAPS67", L"AlphaPlayer34517", L"ArcticAndroid", L"AlwaysOnTime38", L"Abaddon74", L"Angrification", L"Anonymutt", L"Alternative74", L"APeelySmoothie", L"Ambrosia0",
        L"AngryRabbit9", L"HippoMagician", L"AngryRoommate10", L"SweetPenguin16", L"Arclite28", L"CarpMedley", L"SaberPhoenix75", L"AthenaOrApollo", L"AtomicTerr\u221A\u2264r", L"AttackOLantern",
        L"AngryDuck51", L"Atomizer5", L"AyeBall23", L"AngelEye30", L"Primefall44", L"Armstrong78", L"Baethal", L"BabySeahorse81", L"StoneDragon61", L"Back2Work61",
        L"DarkTarget12", L"Bag0Hammers", L"GerbilTrapeze", L"BadKahuna94", L"BananaPastrami", L"Banandana82", L"BattleHammer7", L"ToxicTropic", L"BagelBoy82", L"Balefire81",
        L"JonesyWannabe46", L"Barktooth81", L"Beebitme", L"SlothLife16", L"BeepGoesBeep", L"Represent82", L"BearPuns55", L"Belltop", L"BertIsNice", L"BeeHive7",
        L"BetaChampion", L"BehindYou60", L"BellyFlop40", L"BioDragoon", L"BeefSupreme35", L"BitOJoy", L"BillyBrilliant23", L"BigBuddy15", L"BlinkImGone44", L"Blizzardbeard77",
        L"BlipBlap53", L"BlindCat47", L"Blueberry1002", L"BoatRider10", L"Boomstick59", L"BoatingIsLife", L"Bobtimus", L"BoldPrediction", L"Blackjack31", L"BonyHumans",
        L"Bongodrum010", L"BobsUrUncle71", L"BootifulSnow", L"BotheredBud1", L"BootThisUp256", L"BoxOfGoblins", L"BorisTheGreat37", L"BrainInvader", L"BroSincerely", L"BraddleRoyale80",
        L"Bro2Max", L"BrandNew47", L"BreadSandwich42", L"BeRooted", L"Brotato3131313", L"Brometheus52", L"BossBoi20", L"BreakerBreaker58", L"Bronson66", L"BrokenRecord26",
        L"CryptoDancer", L"BubbleDragons", L"BubblyFalcon", L"Nocturne9964", L"BrutalNoodle91", L"BatTacos", L"Brutalis42", L"Buffering5", L"BumbleB3ar", L"Bundledbats",
        L"BurgerGuy1011", L"PuppyDance9", L"BusinessJoe", L"ButchSquire", L"BuddyGuy9518", L"BushCamper2477", L"butfirstCOFFEE67", L"ButternutQuash", L"BewareTheGOAT", L"ButterKid24",
        L"C-Block54", L"CactusDad80", L"SublimeCactus24", L"CafeDave58", L"CandyPlumber", L"CovertClown95", L"CannotDo42", L"CaptainKiernan", L"CantTouchMe67", L"CantHearYou3",
        L"CarneAsideOf", L"Skyfire85", L"Carcrusher94", L"Caspian79", L"Cathode57", L"Centice49", L"BlackCat61", L"CherubTree", L"Channel2659", L"Ch0pp3r66",
        L"CharmedImSure85", L"ChestOfFault", L"Cannonball86", L"Primetime71", L"Cheese10128", L"iHazHighGround", L"ChickenFarmer17", L"Lexington90", L"ChiliDog87", L"ChillMono",
        L"Chorizo1337", L"Chronicled5", L"Chug_Brigade", L"ChArter52", L"Chronolink24", L"FootballDay", L"ColonelChugJug", L"CleverPanda91", L"Chipper70", L"CleverPun92",
        L"Cleopatra30", L"CoffeeFueled84", L"ConstantThorn", L"CounterMantra", L"ColonelFox41", L"ComingForYou19", L"CopperBottom43", L"Corrosion86", L"CrabInMyShoe", L"CowJuice92",
        L"CrepeSalad", L"CrazyJet83", L"CopyKat58", L"CrazyPea96", L"CrocodileBard", L"CruiseOn", L"CrabbyBaby71", L"Crestfallen64", L"Crypt1c60", L"CtrlAltDeleted94",
        L"CyborgMelon", L"CrumbBum91", L"Curveball20", L"d0m3dw3ll3r", L"N0nDa1ry", L"CyberWolf26", L"DanglingKeys", L"DarlingSnail", L"D3m0nk3y56", L"Dawson201",
        L"Damarus83", L"DanIsHere50", L"DeepStealth", L"DefiantGerbil", L"DisIsMatt", L"DekesNdangles", L"2Xtrouble13", L"DemonOnWheels", L"BraunyBanana", L"Dinosaur4Hire",
        L"DJHoneybun", L"Dessloch52", L"Dimitra66", L"DiamondQuartz78", L"Disconnect21", L"Divisor78", L"Caveman59", L"DoctorGungHo", L"DrQuikMaths", L"PopNLock7",
        L"Doilies4Days", L"DoctorLobby92", L"Strider0728", L"DoublePinpoint", L"BestDoggo83", L"DoubleDaring", L"DoubleBlitz52", L"DoubleDuel75", L"DoublePumpkin78", L"DoubleRainbow96",
        L"DrPlanet", L"DoUeven84", L"Dreamdunker", L"DramaAlert5", L"Doyourdishes33", L"Drekorig97", L"DJLightning85", L"Drimacus7", L"DrPlants40", L"Dunkball96",
        L"DeluxeDino18", L"DreamDog18", L"EarOfCorn1010", L"Eggwithnosalt", L"EdgeGoose", L"HyenaBrigade", L"GoldenOctopus0", L"LazyLlama7", L"KeyboardKing32", L"JimmyIsOnFire97",
        L"SadTroll94", L"Elftastic", L"Erraatic27", L"Eubanker555", L"ElderFly13", L"Exhale34", L"EndlessMike52", L"F1veFruits", L"EvilJonesy12", L"Fall3nSlasher",
        L"ElPollo85", L"FacelessPanda56", L"FauxFeathers", L"FallingInStyle86", L"FancyFingers87", L"FantasyCruz98", L"PositiveFeels", L"Faraleth90", L"Fictionpoint", L"FavoriteFlavor45",
        L"FightinFishBoy", L"FinlessShark", L"FeasibleWeasel80", L"TimeSplat", L"FishingEvryDay", L"FireCheeks56", L"FireDynasty68", L"5HoleSniper", L"FlamingoGuru", L"FishWithLungs27",
        L"FlankTheTank", L"FlavorCapitan", L"SomethingFishy3", L"FlawlessGame", L"FluffySardine", L"Flyingfender", L"FootBarn", L"Firewall4", L"FlossPatrol82", L"Forge62018",
        L"ForksNSporks", L"ForlornMorning", L"FluffyMuffin8", L"ForsakenVoyage", L"Forbidden60", L"ForkAndKnife17", L"ForkSpoonKnife42", L"Fortifiable", L"FreeCookies", L"FortGoodNite43",
        L"FortniteKid34558", L"Frediculous20", L"FreeMinded49", L"Freylis21", L"FriendlySniper27", L"FrogSpawn42", L"FunkLogic17", L"FunkyFresh1349", L"Frostbitten86", L"FuzzyPickl3s",
        L"g0tmy3Y3onU", L"FuriousBug50", L"GalaxyBrain77", L"GamerGreat30", L"GameWinnerz87", L"Gaargod55", L"Glamour67", L"FunSized86", L"Freefire63", L"GeekTragedy",
        L"GeoForts", L"GiantDaisy", L"Goosezilla13", L"Gatordile81", L"GiaPuppy", L"GhostColour6", L"GigaWatt10k", L"GimmeCrisps", L"GimmeMoar", L"FancyPumpkin87",
        L"GlassCannister", L"GlitterMantis", L"GhostChicken12", L"Giuseppina71", L"Gkublok13", L"GloatingEmu", L"Glopazoid", L"GlowBeetle41", L"GnomeOverlord", L"FlimsyGoat",
        L"GoblinSelfie", L"GnomeRider16", L"GoldenCardamom", L"Goldie971", L"GoldenMage12", L"GoldenManatee6", L"GolfVoid", L"Going4Gold17", L"GoneFishin15", L"GardenDolphin",
        L"GradedOnACurve", L"GotHeartburn", L"gonnagetcha15", L"Gooddoggo80", L"Gigabyte51", L"GreasyGoal82", L"Greedy33", L"Initialize0", L"Dominator25", L"Gridl0cked",
        L"Grandma40", L"GrannyGunner3", L"GreatGriffin28", L"GoodDawg13", L"GrumbleofPugs6", L"GrimRanger72", L"Guacamoldy7", L"GuardianHeart4", L"GuessWhat23", L"Gurshwunner",
        L"H1pC0c0nut", L"HaaalpMe", L"GunGoesPewPew38", L"HangryBuddy", L"HamsterTaxi", L"Hydrohawk7", L"HaHaHeeHee73", L"HalfEatenPizza82", L"HandsomeLemon34", L"HashTagonistic",
        L"HauntedDreams", L"HappyMelon50", L"HazardousPaste", L"HashtagToad57", L"HeliumHog", L"Here4Dancing", L"HawkWrath74", L"HermitCrab26", L"Hevymeddle", L"PlzBeNiceToMe",
        L"HopOnDown20", L"HelloMyDude54", L"Hashbrown31", L"HollowBrick5", L"HeyThereFriend81", L"MajesticMutant", L"HeroicHealer71", L"Hoodwinked12", L"Hiveminder97", L"HotelBlankets",
        L"HowAreMy90s", L"Horse4227", L"HubcapDonuts", L"SpringChicken10", L"ISeeCiabatta", L"Howitzer98", L"iAteMySpacebar", L"IAmW4t3r", L"IronHype58", L"HomeSkillet95",
        L"Hollywood70", L"IckyWombat", L"ifyougotem", L"IGottaGoToBed", L"IceFork43", L"Ichthys25", L"IHaveFullBrick", L"IGotSticks85", L"IhaveAGoodElo", L"IHaveFullMetal",
        L"IHateBacon35", L"ImGonnaWin2234", L"InflatedCouch", L"InsideAgent", L"LetsBePals23", L"ImFam0us24", L"InfinityPigeon66", L"ImReallyADog76", L"IntoTheOcean", L"InverseUnicorn95",
        L"IronBoss86", L"IrritatedIris", L"IsBruceHome", L"InnoScent24", L"IronMade20", L"Irontide57", L"SalsaVerde50", L"SilentRogue98", L"IvanDude0001", L"IsOnlyGame12",
        L"IamPizza5", L"BillysMom17", L"ElectricKitten12", L"MarathonWalker", L"LostYeti57", L"MagicJackie8", L"JebidiahSparks", L"VeryMuchGG", L"TallestAnt4", L"JewelOfWisdom",
        L"JellyBeats81", L"JollyGoat46", L"JustSugar85", L"JonnyTheKid52", L"JonesyForever35", L"ImmortalMango66", L"Happy2Bhere39", L"JumpyMcJump64", L"JackalViper", L"JustBuster73",
        L"TermiteMuscles", L"KenCrunch", L"KeyFlamingo", L"VerticalSlice", L"Juxtapose48", L"KatKat200239", L"WigglyCow", L"KingFlexor", L"SimplyKraken", L"KeepScrolling95",
        L"KingOfFudge", L"ThroneBlade47", L"KimchiFighter13", L"BoogieKing85", L"Knowaguy", L"KingToucan2", L"KingKoala82", L"FickleKnight20", L"DoomCrow74", L"ColorfulCoyote",
        L"Kragoth93", L"Quack4Bread", L"Kregore73", L"Shadow74739", L"LambFury", L"LavaHound68", L"JungleProwler25", L"1337Sk1lz", L"Landsnail80", L"LarsTheKid25",
        L"Laserfrog44", L"ManiacMeerkat49", L"JustABitEpic", L"LewtGoblin7", L"Liftoff36", L"Lightsp33d79", L"LilBandit94", L"LilGrande56", L"LittlePidgeon", L"LaidBackBeats",
        L"LousyCentaur", L"LovelyLemur", L"LobbyBound43", L"LilMeowMeow50", L"DangerLeopard", L"LootTrooper51", L"Mayonnaise48", L"LadyLuck75", L"Lulabyebye", L"LuckyNumber4279",
        L"LuckyLadybug65", L"MagicQuail", L"MaximumMad53", L"Lizardfish49", L"MajorProdigy51", L"majorOOF49", L"MadBrad50", L"Makreth4", L"Mandible13", L"Manwell65",
        L"MarineDog54", L"Malakai96", L"MaximumBro3", L"MaximusIon59", L"MaybeNot40", L"MeanSardine", L"KittyCat80", L"McCucumber71", L"McLawnmower91", L"MegaYeet98",
        L"MemoryGobbler", L"Megacrash21", L"Meshuncle", L"MeLlamoJefe76", L"Meshdaddy85", L"MightyMonkfish", L"MightySobek", L"MidnightScar26", L"Memphis31", L"Marionette84",
        L"Thx4DaLoot14", L"MeowMeow38", L"MissedJumpshot", L"Mission1536", L"MintElephant26", L"RareOrange", L"MisterTaboo", L"MisterMosquito0", L"MongLason", L"MooseRampage",
        L"UltraMom83", L"MondayMourning82", L"MoreOfTheSame", L"MoonWizard76", L"MsFroggy123", L"Monolith95", L"Mouthful95", L"SpeedSparrow47", L"MrBurgers39", L"MayorMVP",
        L"MuddyBoots7", L"Nanochunks", L"JokeFountain", L"SwampRhino", L"MutantGecko7", L"NavyQuail", L"Nashtyboy67", L"NeoDynamic", L"Fearmaster62", L"Mountain83",
        L"NeonSalmon", L"NegativeNight7", L"NocturnePatrol", L"NomnomCarver", L"MysteryMan79", L"NeoHero4", L"NonCarbon8", L"ZeroOverDrive", L"ElegantBeast39", L"TooManyBeets",
        L"NotMyLasagna18", L"Numbers789", L"Nickolai62", L"NoodleSoup83", L"Number141", L"OblongWidget", L"NumeroDos28", L"OddComplex85", L"OldWaterBottle28", L"OleReliable78",
        L"OneMillion17", L"OneGlimpse87", L"OnlyMust4rd", L"OneMillionAnts24", L"OpenShutCase", L"OpenSeason23", L"OrangeTango33", L"Overtime46", L"TroubledTaco45", L"9000Hippos",
        L"Ouboudah25", L"OxfordCommas", L"ItsAlwaysMe", L"Othello54", L"Ophelia66", L"Paracord", L"PopcornSalt3", L"PapaParadise82", L"ParanoidCactus", L"PebbleSpider",
        L"NotAPalindrome38", L"PapaJimbo9", L"PepperTaxi", L"PeskyInfant", L"PickaxeUAQ", L"FrogTheCat18", L"Smoothie66", L"PizzaUnic0rn", L"PerpleXed79", L"Pinnacle6",
        L"PlatinumPony", L"PirateWizard22", L"PlayOnRepeat", L"Playmaker365", L"PlasticBullets62", L"PoachedPepper", L"PortableOx", L"PortaSniper", L"PLZST0P75", L"pleaseletmewin10",
        L"PotatoCupid", L"ViciousPie44", L"PowerFlank", L"PrancingPwnee", L"PrimeSystem", L"PrinceWombat", L"PrincessImp", L"PotsNPans39", L"Prisoner19A", L"PsyonicOx",
        L"PurpleRocks99", L"QuartetUltra", L"QuantumFig", L"PumpkinKnight83", L"PureSneak92", L"QuadAlex13", L"QWERTYb0ss", L"R00kMine6", L"RabbitKicks", L"PurpleCrayon85",
        L"R0ckStar5", L"Ramsbottle", L"RainbowZebra36", L"RandoDog73", L"Rainmaker67", L"Ramp4ge71", L"RavensRevenge13", L"RatherBFishin0", L"RealMisfortune", L"RelaxOkay",
        L"Reach4TheSky54", L"RedOrange2240", L"Rhett5408", L"RiskyDigits", L"RagePower81", L"Riftmage54", L"Riboflavin52", L"RoarTiger24", L"GoodRobin6", L"RoyaleCorral",
        L"RocketBoi87", L"Rocking7614", L"RudeAwaken1ngz", L"Roundhouse47", L"SalemSquid", L"RustyShine93", L"SLUGshot65", L"Sabretooth67", L"Salamander8910", L"Sandra6000",
        L"SandSpider2000", L"SalsaPants36", L"SaltySandwich81", L"SappyPirate", L"Sandwitch90", L"SassyJazzhands", L"ScorpionOne50", L"UnderwaterSpy77", L"SeagullTamer", L"BitterSecret",
        L"SeeUFriday", L"Gearhead46", L"SergeantSummer", L"SeriousSteve35", L"Sevarog3", L"Seven8Nine93", L"ShadowArrow58", L"Sapphire59", L"SilverBear89", L"BubbleHunter22",
        L"ShieldHorse63", L"Shepard52", L"SwampJackal", L"ShiniestBovine", L"ShogunGiraffe", L"Brainshocked0", L"Shoot2Thrill25", L"Shotdrop87", L"ShootyMcGee40", L"ShoTTy4Life48",
        L"Sharkboy61", L"Shotty2thebody66", L"ShottyBOI1", L"SickPapaya", L"ShrimpHeaven74", L"Shrimpz62", L"SilverySliver", L"SilentBanana17", L"SimplyTheBest97", L"SirChimera83",
        L"SirTricksALot21", L"SizzlinChamp", L"SixesAndSevens65", L"SkeletonMonkey14", L"Slammie9", L"sleepingmantis47", L"SlickElephant", L"SleepyPete56", L"SMGsRdaBest", L"Smellwonk",
        L"Pickles60", L"SlurpItUp93", L"Sn00tyMagician", L"SneakyBush9898", L"SniperCharlie92", L"JakeTheSnake7770", L"SnipersDelight50", L"Reaper1487", L"SmittenKitten4", L"GetScoped92",
        L"SoFastMan", L"Sniper36094", L"Snowday95", L"Solocamper8", L"SoggyCookie26", L"Renegade53", L"SoloSpy86", L"SonicComet37", L"SoulIntellect", L"SoSweaty63",
        L"SoulSpecter", L"SpaceRivers", L"BridgeBuilder87", L"Soundthealarm18", L"SpaceChimp18", L"SpaceLight13", L"SpaghettiCode77", L"SpiderMitten", L"SpokenWard", L"SpookyLeader21",
        L"SpeedyTheCat98", L"SquadMaster22", L"StaleGrapes", L"SporkMaster20", L"StarryGolds", L"SquadUp98", L"Squar3R00t5", L"AtTheBeach321", L"StarWarden80", L"StayHydrated41",
        L"SteelGoose18", L"StoicAlmond", L"TheFreezer21", L"StickyFingers60", L"Sugartech", L"StrangePlant87", L"StringCheese15", L"Sundown57", L"Subversa39", L"GoodIRL",
        L"SuperPoshGrape", L"SuperCandyGurl77", L"Syrupmancer", L"TacoArmada", L"SuperDogCat43", L"SurfRider10146", L"Swabbie23", L"TacticalFish64", L"TacoHands33", L"TAgYOuRIt9",
        L"TaterTots6789", L"Tamerlan30", L"Telepwn78", L"TearCollector18", L"Telltool", L"TankBro85", L"M0arMinutes", L"TepidPorridge", L"FroyoLicker", L"ThatHURTS30",
        L"DangerZero32", L"Thannis80", L"QueenBeet74", L"CatsClause", L"TheBestThereIs45", L"Tacocat60", L"FenderBlender7", L"TheFlopperKing", L"MorninWaffles", L"FishermanJoe86",
        L"MyNameIsHelga15", L"SlurpMaster650", L"OnetrickWonder64", L"Stinger85", L"WindedCrusader", L"Monkeyking17", L"ThreeJabLad", L"NullAndVoid58", L"Notatest22", L"ThermalDragon39",
        L"ThisSideUp87", L"GunShowTickets", L"TigerDreads", L"ThunderKarma33", L"WowerZower", L"TinyMcLarge", L"TickTickBoom57", L"Tigertooth91", L"Time2Go80", L"TLDR4life",
        L"TigerSmash3", L"TinyMoose50", L"DrumGunnar", L"TopDanger28", L"TotallyJeff", L"TreesapBetty", L"BestRegards81", L"Triangulated86", L"TryAgain49", L"FreshFish71",
        L"TurtleUp12", L"TwoHitWonder97", L"TwoWeeks18", L"UndeadBadger", L"UltimateSteve26", L"Valley6789", L"UltraDavid26", L"Untouchable92", L"Mascot2458", L"Vectron29",
        L"PureBacon94", L"VeggieBurger24", L"WaffleDoctor", L"VisionRunner18", L"WanderAgent", L"CyborgWalrus6", L"WalkInThePark66", L"WildfireWizard", L"WalkTheDog80", L"Vertigo70",
        L"Warning1496", L"WildCactusBob", L"Warsiren14", L"Whitman40", L"WeekendWarrior81", L"WickedWasp10", L"Wildonkey", L"Wondertail", L"WrathofKnight", L"Wolfman11",
        L"WolfPhantom46", L"WiggleShark28", L"XenoBotanist27", L"XxBonemealxX", L"xX360NoScopeXx28", L"xXAnonymousXx30", L"Yarley0001", L"HoorayBeets", L"XxCoolNamexX63", L"XxR3AP3RxX64",
        L"YoureNotYour", L"YeetLife35", L"YellowSauce80", L"YourBestFriend63", L"UltimateZenith14", L"Bubbles41", L"Yeetman57", L"Scorpio62", L"brun026", L"leoz1nnn1",
        L"JoyxxxRJ", L"XsniperzX1", L"royaleking15", L"loco^das^lhamas", L"segura_meu_HS", L"M\u00E1nu_M\u00E1zinha", L"capivara16", L"TheM\u00F4nia1", L"cau\u00EA_sp_97", L"juliano\u00CD\u00CD\u00CDBR",
        L"dani_gato1", L"jogadorsemnome05", L"marc\u00E3oBA_BR", L"Jo\u00E3o_SP", L"Extrupicio3", L"semnome0", L"torpekills", L"pAnDoRa10", L"n00bk1ller", L"tligtlig",
        L"lek_rj_br", L"2jo\u00E3ozinho_bh2", L"carniceir00", L"XXplosiv00", L"topzer@@@", L"k1\u00ED\u00ED3R", L"filipinh0sp", L"mr_jonjon", L"zzPAPAICHEGOUzz", L"kt1r0_100",
        L"biel~30", L"chinela_voadora1", L"`tigredoagreste`", L"&darkBeast&", L"XxdivadaskllxX", L"pulgaPG07", L"dead_docks", L"x0baiacubolad0x", L"PatatinoSalvahe", L"DestruyenFritas",
        L"disloka01996", L"xXxdark_tonixXx", L"pedrileanguile", L"AngryMiau", L"locodelcoco8836", L"asdruval_18", L"pepinovski92", L"se\u00F1orsentin", L"ginga871", L"detrasdeti777",
        L"pimpampum2019", L"crazycharly992", L"burlaosquad99", L"Lallamaquellama1", L"illomenuohersot", L"M1st3r0wn3r", L"Rrrrumba98", L"dante_killer12", L"MrPiumPium", L"LordHamburguesa1",
        L"Aplastafritas", L"El_Pr098", L"PatatasHazard", L"ZeroHamburgesa", L"Robak1lls", L"Chimuelopr0", L"Pr0K1ller07", L"NotDefaultPlayer", L"UltimatePr0Play", L"LlamaSpecialist0",
        L"Best_Llama_Ever", L"PatatasScope", L"JustAnotherPro42", L"CrazyAlpaca98", L"gingerbunny125", L"pineappleghost777", L"D34THBR1NG3R12", L"Deadly_Sparkles", L"MangeDlaPoutine", L"Scream4Poutine",
        L"MaisVoyonsDont"
    } },
    { "PhoebeNames_EU", {
        L"0PingSting", L"12Siblings12", L"2xPump4Life", L"_BigBainPlayz_", L"-=Fr0stDevil=-", L"1eyedInvent0r", L"360walkaway", L"42Wow47", L"2sweaty23", L"0mn1v0r378",
        L"PanicButton47", L"1Forest120", L"8BallWasHere", L"4Regionals", L"90sRunner", L"A Sweaty Dog", L"SpectrumKnight13", L"aaaaaaaaalbert", L"4tnite4ever15", L"aaaaaleeex",
        L"ADudeNamedPhil", L"90cranker7", L"AGenuineHuman", L"ActivateFail", L"aClassyGamer94", L"Agoraphone-8", L"AGoodOmen", L"Agua_Cat", L"Ahat4aMouse", L"AggroHamster59",
        L"Aim Like Idaho", L"Albacore6", L"airplanelegs", L"AligatorPear", L"TheBe$t", L"Allsmyles:D", L"AllTheCandy", L"Archibard11", L"amygdallama", L"AmpleBlindspot",
        L"ALLCAPS67", L"AlphaPlayer34517", L"ArcticAndroid", L"AlwaysOnTime38", L"Abaddon74", L"Angrification", L"Anonymutt", L"Alternative74", L"APeelySmoothie", L"Ambrosia0",
        L"AngryRabbit9", L"HippoMagician", L"AngryRoommate10", L"SweetPenguin16", L"Arclite28", L"CarpMedley", L"SaberPhoenix75", L"AthenaOrApollo", L"AtomicTerr\u221A\u2264r", L"AttackOLantern",
        L"AngryDuck51", L"Atomizer5", L"AyeBall23", L"AngelEye30", L"Primefall44", L"Armstrong78", L"Baethal", L"BabySeahorse81", L"StoneDragon61", L"Back2Work61",
        L"DarkTarget12", L"Bag0Hammers", L"GerbilTrapeze", L"BadKahuna94", L"BananaPastrami", L"Banandana82", L"BattleHammer7", L"ToxicTropic", L"BagelBoy82", L"Balefire81",
        L"JonesyWannabe46", L"Barktooth81", L"Beebitme", L"SlothLife16", L"BeepGoesBeep", L"Represent82", L"BearPuns55", L"Belltop", L"BertIsNice", L"BeeHive7",
        L"BetaChampion", L"BehindYou60", L"BellyFlop40", L"BioDragoon", L"BeefSupreme35", L"BitOJoy", L"BillyBrilliant23", L"BigBuddy15", L"BlinkImGone44", L"Blizzardbeard77",
        L"BlipBlap53", L"BlindCat47", L"Blueberry1002", L"BoatRider10", L"Boomstick59", L"BoatingIsLife", L"Bobtimus", L"BoldPrediction", L"Blackjack31", L"BonyHumans",
        L"Bongodrum010", L"BobsUrUncle71", L"BootifulSnow", L"BotheredBud1", L"BootThisUp256", L"BoxOfGoblins", L"BorisTheGreat37", L"BrainInvader", L"BroSincerely", L"BraddleRoyale80",
        L"Bro2Max", L"BrandNew47", L"BreadSandwich42", L"BeRooted", L"Brotato3131313", L"Brometheus52", L"BossBoi20", L"BreakerBreaker58", L"Bronson66", L"BrokenRecord26",
        L"CryptoDancer", L"BubbleDragons", L"BubblyFalcon", L"Nocturne9964", L"BrutalNoodle91", L"BatTacos", L"Brutalis42", L"Buffering5", L"BumbleB3ar", L"Bundledbats",
        L"BurgerGuy1011", L"PuppyDance9", L"BusinessJoe", L"ButchSquire", L"BuddyGuy9518", L"BushCamper2477", L"butfirstCOFFEE67", L"ButternutQuash", L"BewareTheGOAT", L"ButterKid24",
        L"C-Block54", L"CactusDad80", L"SublimeCactus24", L"CafeDave58", L"CandyPlumber", L"CovertClown95", L"CannotDo42", L"CaptainKiernan", L"CantTouchMe67", L"CantHearYou3",
        L"CarneAsideOf", L"Skyfire85", L"Carcrusher94", L"Caspian79", L"Cathode57", L"Centice49", L"BlackCat61", L"CherubTree", L"Channel2659", L"Ch0pp3r66",
        L"CharmedImSure85", L"ChestOfFault", L"Cannonball86", L"Primetime71", L"Cheese10128", L"iHazHighGround", L"ChickenFarmer17", L"Lexington90", L"ChiliDog87", L"ChillMono",
        L"Chorizo1337", L"Chronicled5", L"Chug_Brigade", L"ChArter52", L"Chronolink24", L"FootballDay", L"ColonelChugJug", L"CleverPanda91", L"Chipper70", L"CleverPun92",
        L"Cleopatra30", L"CoffeeFueled84", L"ConstantThorn", L"CounterMantra", L"ColonelFox41", L"ComingForYou19", L"CopperBottom43", L"Corrosion86", L"CrabInMyShoe", L"CowJuice92",
        L"CrepeSalad", L"CrazyJet83", L"CopyKat58", L"CrazyPea96", L"CrocodileBard", L"CruiseOn", L"CrabbyBaby71", L"Crestfallen64", L"Crypt1c60", L"CtrlAltDeleted94",
        L"CyborgMelon", L"CrumbBum91", L"Curveball20", L"d0m3dw3ll3r", L"N0nDa1ry", L"CyberWolf26", L"DanglingKeys", L"DarlingSnail", L"D3m0nk3y56", L"Dawson201",
        L"Damarus83", L"DanIsHere50", L"DeepStealth", L"DefiantGerbil", L"DisIsMatt", L"DekesNdangles", L"2Xtrouble13", L"DemonOnWheels", L"BraunyBanana", L"Dinosaur4Hire",
        L"DJHoneybun", L"Dessloch52", L"Dimitra66", L"DiamondQuartz78", L"Disconnect21", L"Divisor78", L"Caveman59", L"DoctorGungHo", L"DrQuikMaths", L"PopNLock7",
        L"Doilies4Days", L"DoctorLobby92", L"Strider0728", L"DoublePinpoint", L"BestDoggo83", L"DoubleDaring", L"DoubleBlitz52", L"DoubleDuel75", L"DoublePumpkin78", L"DoubleRainbow96",
        L"DrPlanet", L"DoUeven84", L"Dreamdunker", L"DramaAlert5", L"Doyourdishes33", L"Drekorig97", L"DJLightning85", L"Drimacus7", L"DrPlants40", L"Dunkball96",
        L"DeluxeDino18", L"DreamDog18", L"EarOfCorn1010", L"Eggwithnosalt", L"EdgeGoose", L"HyenaBrigade", L"GoldenOctopus0", L"LazyLlama7", L"KeyboardKing32", L"JimmyIsOnFire97",
        L"SadTroll94", L"Elftastic", L"Erraatic27", L"Eubanker555", L"ElderFly13", L"Exhale34", L"EndlessMike52", L"F1veFruits", L"EvilJonesy12", L"Fall3nSlasher",
        L"ElPollo85", L"FacelessPanda56", L"FauxFeathers", L"FallingInStyle86", L"FancyFingers87", L"FantasyCruz98", L"PositiveFeels", L"Faraleth90", L"Fictionpoint", L"FavoriteFlavor45",
        L"FightinFishBoy", L"FinlessShark", L"FeasibleWeasel80", L"TimeSplat", L"FishingEvryDay", L"FireCheeks56", L"FireDynasty68", L"5HoleSniper", L"FlamingoGuru", L"FishWithLungs27",
        L"FlankTheTank", L"FlavorCapitan", L"SomethingFishy3", L"FlawlessGame", L"FluffySardine", L"Flyingfender", L"FootBarn", L"Firewall4", L"FlossPatrol82", L"Forge62018",
        L"ForksNSporks", L"ForlornMorning", L"FluffyMuffin8", L"ForsakenVoyage", L"Forbidden60", L"ForkAndKnife17", L"ForkSpoonKnife42", L"Fortifiable", L"FreeCookies", L"FortGoodNite43",
        L"FortniteKid34558", L"Frediculous20", L"FreeMinded49", L"Freylis21", L"FriendlySniper27", L"FrogSpawn42", L"FunkLogic17", L"FunkyFresh1349", L"Frostbitten86", L"FuzzyPickl3s",
        L"g0tmy3Y3onU", L"FuriousBug50", L"GalaxyBrain77", L"GamerGreat30", L"GameWinnerz87", L"Gaargod55", L"Glamour67", L"FunSized86", L"Freefire63", L"GeekTragedy",
        L"GeoForts", L"GiantDaisy", L"Goosezilla13", L"Gatordile81", L"GiaPuppy", L"GhostColour6", L"GigaWatt10k", L"GimmeCrisps", L"GimmeMoar", L"FancyPumpkin87",
        L"GlassCannister", L"GlitterMantis", L"GhostChicken12", L"Giuseppina71", L"Gkublok13", L"GloatingEmu", L"Glopazoid", L"GlowBeetle41", L"GnomeOverlord", L"FlimsyGoat",
        L"GoblinSelfie", L"GnomeRider16", L"GoldenCardamom", L"Goldie971", L"GoldenMage12", L"GoldenManatee6", L"GolfVoid", L"Going4Gold17", L"GoneFishin15", L"GardenDolphin",
        L"GradedOnACurve", L"GotHeartburn", L"gonnagetcha15", L"Gooddoggo80", L"Gigabyte51", L"GreasyGoal82", L"Greedy33", L"Initialize0", L"Dominator25", L"Gridl0cked",
        L"Grandma40", L"GrannyGunner3", L"GreatGriffin28", L"GoodDawg13", L"GrumbleofPugs6", L"GrimRanger72", L"Guacamoldy7", L"GuardianHeart4", L"GuessWhat23", L"Gurshwunner",
        L"H1pC0c0nut", L"HaaalpMe", L"GunGoesPewPew38", L"HangryBuddy", L"HamsterTaxi", L"Hydrohawk7", L"HaHaHeeHee73", L"HalfEatenPizza82", L"HandsomeLemon34", L"HashTagonistic",
        L"HauntedDreams", L"HappyMelon50", L"HazardousPaste", L"HashtagToad57", L"HeliumHog", L"Here4Dancing", L"HawkWrath74", L"HermitCrab26", L"Hevymeddle", L"PlzBeNiceToMe",
        L"HopOnDown20", L"HelloMyDude54", L"Hashbrown31", L"HollowBrick5", L"HeyThereFriend81", L"MajesticMutant", L"HeroicHealer71", L"Hoodwinked12", L"Hiveminder97", L"HotelBlankets",
        L"HowAreMy90s", L"Horse4227", L"HubcapDonuts", L"SpringChicken10", L"ISeeCiabatta", L"Howitzer98", L"iAteMySpacebar", L"IAmW4t3r", L"IronHype58", L"HomeSkillet95",
        L"Hollywood70", L"IckyWombat", L"ifyougotem", L"IGottaGoToBed", L"IceFork43", L"Ichthys25", L"IHaveFullBrick", L"IGotSticks85", L"IhaveAGoodElo", L"IHaveFullMetal",
        L"IHateBacon35", L"ImGonnaWin2234", L"InflatedCouch", L"InsideAgent", L"LetsBePals23", L"ImFam0us24", L"InfinityPigeon66", L"ImReallyADog76", L"IntoTheOcean", L"InverseUnicorn95",
        L"IronBoss86", L"IrritatedIris", L"IsBruceHome", L"InnoScent24", L"IronMade20", L"Irontide57", L"SalsaVerde50", L"SilentRogue98", L"IvanDude0001", L"IsOnlyGame12",
        L"IamPizza5", L"BillysMom17", L"ElectricKitten12", L"MarathonWalker", L"LostYeti57", L"MagicJackie8", L"JebidiahSparks", L"VeryMuchGG", L"TallestAnt4", L"JewelOfWisdom",
        L"JellyBeats81", L"JollyGoat46", L"JustSugar85", L"JonnyTheKid52", L"JonesyForever35", L"ImmortalMango66", L"Happy2Bhere39", L"JumpyMcJump64", L"JackalViper", L"JustBuster73",
        L"TermiteMuscles", L"KenCrunch", L"KeyFlamingo", L"VerticalSlice", L"Juxtapose48", L"KatKat200239", L"WigglyCow", L"KingFlexor", L"SimplyKraken", L"KeepScrolling95",
        L"KingOfFudge", L"ThroneBlade47", L"KimchiFighter13", L"BoogieKing85", L"Knowaguy", L"KingToucan2", L"KingKoala82", L"FickleKnight20", L"DoomCrow74", L"ColorfulCoyote",
        L"Kragoth93", L"Quack4Bread", L"Kregore73", L"Shadow74739", L"LambFury", L"LavaHound68", L"JungleProwler25", L"1337Sk1lz", L"Landsnail80", L"LarsTheKid25",
        L"Laserfrog44", L"ManiacMeerkat49", L"JustABitEpic", L"LewtGoblin7", L"Liftoff36", L"Lightsp33d79", L"LilBandit94", L"LilGrande56", L"LittlePidgeon", L"LaidBackBeats",
        L"LousyCentaur", L"LovelyLemur", L"LobbyBound43", L"LilMeowMeow50", L"DangerLeopard", L"LootTrooper51", L"Mayonnaise48", L"LadyLuck75", L"Lulabyebye", L"LuckyNumber4279",
        L"LuckyLadybug65", L"MagicQuail", L"MaximumMad53", L"Lizardfish49", L"MajorProdigy51", L"majorOOF49", L"MadBrad50", L"Makreth4", L"Mandible13", L"Manwell65",
        L"MarineDog54", L"Malakai96", L"MaximumBro3", L"MaximusIon59", L"MaybeNot40", L"MeanSardine", L"KittyCat80", L"McCucumber71", L"McLawnmower91", L"MegaYeet98",
        L"MemoryGobbler", L"Megacrash21", L"Meshuncle", L"MeLlamoJefe76", L"Meshdaddy85", L"MightyMonkfish", L"MightySobek", L"MidnightScar26", L"Memphis31", L"Marionette84",
        L"Thx4DaLoot14", L"MeowMeow38", L"MissedJumpshot", L"Mission1536", L"MintElephant26", L"RareOrange", L"MisterTaboo", L"MisterMosquito0", L"MongLason", L"MooseRampage",
        L"UltraMom83", L"MondayMourning82", L"MoreOfTheSame", L"MoonWizard76", L"MsFroggy123", L"Monolith95", L"Mouthful95", L"SpeedSparrow47", L"MrBurgers39", L"MayorMVP",
        L"MuddyBoots7", L"Nanochunks", L"JokeFountain", L"SwampRhino", L"MutantGecko7", L"NavyQuail", L"Nashtyboy67", L"NeoDynamic", L"Fearmaster62", L"Mountain83",
        L"NeonSalmon", L"NegativeNight7", L"NocturnePatrol", L"NomnomCarver", L"MysteryMan79", L"NeoHero4", L"NonCarbon8", L"ZeroOverDrive", L"ElegantBeast39", L"TooManyBeets",
        L"NotMyLasagna18", L"Numbers789", L"Nickolai62", L"NoodleSoup83", L"Number141", L"OblongWidget", L"NumeroDos28", L"OddComplex85", L"OldWaterBottle28", L"OleReliable78",
        L"OneMillion17", L"OneGlimpse87", L"OnlyMust4rd", L"OneMillionAnts24", L"OpenShutCase", L"OpenSeason23", L"OrangeTango33", L"Overtime46", L"TroubledTaco45", L"9000Hippos",
        L"Ouboudah25", L"OxfordCommas", L"ItsAlwaysMe", L"Othello54", L"Ophelia66", L"Paracord", L"PopcornSalt3", L"PapaParadise82", L"ParanoidCactus", L"PebbleSpider",
        L"NotAPalindrome38", L"PapaJimbo9", L"PepperTaxi", L"PeskyInfant", L"PickaxeUAQ", L"FrogTheCat18", L"Smoothie66", L"PizzaUnic0rn", L"PerpleXed79", L"Pinnacle6",
        L"PlatinumPony", L"PirateWizard22", L"PlayOnRepeat", L"Playmaker365", L"PlasticBullets62", L"PoachedPepper", L"PortableOx", L"PortaSniper", L"PLZST0P75", L"pleaseletmewin10",
        L"PotatoCupid", L"ViciousPie44", L"PowerFlank", L"PrancingPwnee", L"PrimeSystem", L"PrinceWombat", L"PrincessImp", L"PotsNPans39", L"Prisoner19A", L"PsyonicOx",
        L"PurpleRocks99", L"QuartetUltra", L"QuantumFig", L"PumpkinKnight83", L"PureSneak92", L"QuadAlex13", L"QWERTYb0ss", L"R00kMine6", L"RabbitKicks", L"PurpleCrayon85",
        L"R0ckStar5", L"Ramsbottle", L"RainbowZebra36", L"RandoDog73", L"Rainmaker67", L"Ramp4ge71", L"RavensRevenge13", L"RatherBFishin0", L"RealMisfortune", L"RelaxOkay",
        L"Reach4TheSky54", L"RedOrange2240", L"Rhett5408", L"RiskyDigits", L"RagePower81", L"Riftmage54", L"Riboflavin52", L"RoarTiger24", L"GoodRobin6", L"RoyaleCorral",
        L"RocketBoi87", L"Rocking7614", L"RudeAwaken1ngz", L"Roundhouse47", L"SalemSquid", L"RustyShine93", L"SLUGshot65", L"Sabretooth67", L"Salamander8910", L"Sandra6000",
        L"SandSpider2000", L"SalsaPants36", L"SaltySandwich81", L"SappyPirate", L"Sandwitch90", L"SassyJazzhands", L"ScorpionOne50", L"UnderwaterSpy77", L"SeagullTamer", L"BitterSecret",
        L"SeeUFriday", L"Gearhead46", L"SergeantSummer", L"SeriousSteve35", L"Sevarog3", L"Seven8Nine93", L"ShadowArrow58", L"Sapphire59", L"SilverBear89", L"BubbleHunter22",
        L"ShieldHorse63", L"Shepard52", L"SwampJackal", L"ShiniestBovine", L"ShogunGiraffe", L"Brainshocked0", L"Shoot2Thrill25", L"Shotdrop87", L"ShootyMcGee40", L"ShoTTy4Life48",
        L"Sharkboy61", L"Shotty2thebody66", L"ShottyBOI1", L"SickPapaya", L"ShrimpHeaven74", L"Shrimpz62", L"SilverySliver", L"SilentBanana17", L"SimplyTheBest97", L"SirChimera83",
        L"SirTricksALot21", L"SizzlinChamp", L"SixesAndSevens65", L"SkeletonMonkey14", L"Slammie9", L"sleepingmantis47", L"SlickElephant", L"SleepyPete56", L"SMGsRdaBest", L"Smellwonk",
        L"Pickles60", L"SlurpItUp93", L"Sn00tyMagician", L"SneakyBush9898", L"SniperCharlie92", L"JakeTheSnake7770", L"SnipersDelight50", L"Reaper1487", L"SmittenKitten4", L"GetScoped92",
        L"SoFastMan", L"Sniper36094", L"Snowday95", L"Solocamper8", L"SoggyCookie26", L"Renegade53", L"SoloSpy86", L"SonicComet37", L"SoulIntellect", L"SoSweaty63",
        L"SoulSpecter", L"SpaceRivers", L"BridgeBuilder87", L"Soundthealarm18", L"SpaceChimp18", L"SpaceLight13", L"SpaghettiCode77", L"SpiderMitten", L"SpokenWard", L"SpookyLeader21",
        L"SpeedyTheCat98", L"SquadMaster22", L"StaleGrapes", L"SporkMaster20", L"StarryGolds", L"SquadUp98", L"Squar3R00t5", L"AtTheBeach321", L"StarWarden80", L"StayHydrated41",
        L"SteelGoose18", L"StoicAlmond", L"TheFreezer21", L"StickyFingers60", L"Sugartech", L"StrangePlant87", L"StringCheese15", L"Sundown57", L"Subversa39", L"GoodIRL",
        L"SuperPoshGrape", L"SuperCandyGurl77", L"Syrupmancer", L"TacoArmada", L"SuperDogCat43", L"SurfRider10146", L"Swabbie23", L"TacticalFish64", L"TacoHands33", L"TAgYOuRIt9",
        L"TaterTots6789", L"Tamerlan30", L"Telepwn78", L"TearCollector18", L"Telltool", L"TankBro85", L"M0arMinutes", L"TepidPorridge", L"FroyoLicker", L"ThatHURTS30",
        L"DangerZero32", L"Thannis80", L"QueenBeet74", L"CatsClause", L"TheBestThereIs45", L"Tacocat60", L"FenderBlender7", L"TheFlopperKing", L"MorninWaffles", L"FishermanJoe86",
        L"MyNameIsHelga15", L"SlurpMaster650", L"OnetrickWonder64", L"Stinger85", L"WindedCrusader", L"Monkeyking17", L"ThreeJabLad", L"NullAndVoid58", L"Notatest22", L"ThermalDragon39",
        L"ThisSideUp87", L"GunShowTickets", L"TigerDreads", L"ThunderKarma33", L"WowerZower", L"TinyMcLarge", L"TickTickBoom57", L"Tigertooth91", L"Time2Go80", L"TLDR4life",
        L"TigerSmash3", L"TinyMoose50", L"DrumGunnar", L"TopDanger28", L"TotallyJeff", L"TreesapBetty", L"BestRegards81", L"Triangulated86", L"TryAgain49", L"FreshFish71",
        L"TurtleUp12", L"TwoHitWonder97", L"TwoWeeks18", L"UndeadBadger", L"UltimateSteve26", L"Valley6789", L"UltraDavid26", L"Untouchable92", L"Mascot2458", L"Vectron29",
        L"PureBacon94", L"VeggieBurger24", L"WaffleDoctor", L"VisionRunner18", L"WanderAgent", L"CyborgWalrus6", L"WalkInThePark66", L"WildfireWizard", L"WalkTheDog80", L"Vertigo70",
        L"Warning1496", L"WildCactusBob", L"Warsiren14", L"Whitman40", L"WeekendWarrior81", L"WickedWasp10", L"Wildonkey", L"Wondertail", L"WrathofKnight", L"Wolfman11",
        L"WolfPhantom46", L"WiggleShark28", L"XenoBotanist27", L"XxBonemealxX", L"xX360NoScopeXx28", L"xXAnonymousXx30", L"Yarley0001", L"HoorayBeets", L"XxCoolNamexX63", L"XxR3AP3RxX64",
        L"YoureNotYour", L"YeetLife35", L"YellowSauce80", L"YourBestFriend63", L"UltimateZenith14", L"Bubbles41", L"Yeetman57", L"Scorpio62", L"ErGabbedano95", L"PierluigiCoiSoldi",
        L"JaiFaimJaiSoif", L"Petrarca_aveva_ragione", L"Botterino55", L"ForeverCacciucco", L"LeTueurDesBois7", L"MangeurDePommes13", L"PizzaKetchupAnanas", L"BaguettePower75", L"\u0641\u0648\u0631\u062A\u0646\u0627\u064A\u062A\u0627\u0648\u064A \u0639\u0644\u0649 \u0637\u0648\u0644", L"\u0623\u0645\u064A\u0631\u0629 \u0627\u0644\u062E\u0644\u064A\u062C",
        L"\u0633\u0639\u064A\u062F\u0648 \u0627\u0644\u0631\u0647\u064A\u0628", L"\u062E\u0648\u0627\u0631\u0632\u0645\u064A \u0627\u0644\u0623\u0644\u0639\u0627\u0628", L"\u0639\u0627\u0635\u0641\u0629 \u0627\u0644\u0635\u062D\u0631\u0627\u062198081", L"bucciadibanana27887", L"\u0645\u062D\u0627\u0631\u0628 \u0627\u0644\u0635\u062D\u0631\u0627\u062184059", L"\u0627\u0644\u0641\u0631\u0639\u0648\u0646 \u0627\u0644\u0639\u0627\u0634\u064231847", L"MaisVoyonsDont", L"\u0646\u062C\u0645 \u0627\u0644\u0646\u062C\u0648\u064541318", L"\u062D\u0645\u0627\u062F\u0647 \u064A\u0644\u0639\u06282081", L"MangeDlaPoutine",
        L"\u0410\u043B\u0435\u043A\u0441\u0435\u0439\u041F\u043E\u0442\u0430\u043F\u043E\u0432\u0438\u0447", L"WillyWaller200654425", L"\u041F\u044E\u0440\u0435\u0448\u043A\u0430\u0421\u0423\u043A\u0440\u043E\u043F\u043E\u043C", L"GranManzo", L"carbonarapower", L"MysterySad", L"MalaMonni", L"\u041F\u0440\u043E100 \u0412\u0430\u043D\u044F22540", L"Kartoffel_skillz", L"DerUnglaublicheJustin",
        L"nabbo8940456", L"NatMat33300", L"SirSkillaLot10694", L"Ces87Lasagna", L"RandomTyp28162", L"LeonLP78511", L"MrKompetenz24728", L"\u0412\u0435\u0440\u043D\u0438\u0433\u043E\u0440\u0430", L"PeterUnlustig55089", L"\u0412\u0435\u0440\u043D\u0438\u0434\u0443\u0431",
        L"\u041F\u043E\u0442\u043E\u0440\u043E\u0447\u0430", L"Marco200033274", L"\u0422\u043E\u0439_\u0449\u043E_\u0432_\u0441\u043A\u0430\u043B\u0456_\u0441\u0438\u0434\u0438\u0442\u044C", L"\u0407\u0436\u0430\u0447\u043E\u043A11211", L"\u0422\u043E\u0439_\u0449\u043E_\u0433\u0440\u0435\u0431\u043B\u0456_\u0440\u0432\u0435", L"\u041A\u043E\u0442\u0438\u0433\u043E\u0440\u043E\u0448\u043A\u043E31445", L"\u041B\u0456\u0441\u043E\u0432\u0438\u043A", L"\u041A\u0443\u0446\u044C", L"\u041F\u0435\u0440\u0435\u043B\u0435\u0441\u043D\u0438\u043A", L"\u041F\u043E\u0442\u0435\u0440\u0447\u0430\u0442\u043A\u043E",
        L"\u0416\u0443\u0440\u0431\u0438\u043D\u043A\u0430", L"\u0412\u043E\u0432\u043A\u041F\u0430\u043D\u0456\u0431\u0440\u0430\u0442", L"\u041F\u0430\u043D\u041A\u043E\u0446\u044C\u043A\u0438\u0439", L"\u041A\u043E\u0437\u0430\u043A\u041C\u0430\u043C\u0430\u0439", L"\u041D\u0435\u043F\u043E\u0437\u0431\u0443\u0432\u043D\u0430\u0411\u0435\u043D\u0442\u0435\u0433\u0430", L"GermoglioDiSole", L"\u0425\u0443\u0445\u043023237", L"\u041C\u043E\u043B\u044C\u0444\u0430\u044039106", L"Pippettoloscherzetto", L"ElfiDiCastagna",
        L"\u041B\u0438\u0441\u041C\u0438\u043A\u0438\u0442\u043040495", L"Gandolfoilbianco", L"latuaGiulietta", L"MesserePanettone", L"iltuoRomeo", L"ReginaldoMaccaroni", L"Railatan65466", L"5p4cc07u770", L"eliteIt4li4", L"RamarroMarrone11528",
        L"L4mboZ3ta", L"pimpmypizza", L"\u0422\u0443\u043A\u0421\u043C\u0435\u0421\u0430\u043C\u043E\u0425\u0430\u0439\u0432\u0435\u0440\u0430", L"B1rr3tt4", L"\u041D\u044F\u043C\u0430\u043C\u0414\u0443\u043C\u0438", L"Minionistheway", L"amicaFrida", L"ScaltritiSuperScaltro", L"HelloPapaguena", L"svangatutto",
        L"ilMirino", L"Carroarmato92", L"tuparlifrancese", L"\u041A\u0440\u0430\u043B\u0438 \u041C\u0430\u0440\u043A\u043E86258", L"Stellardente", L"Mery91Raul", L"SuperMati58047", L"LadyValentina79947", L"CaplassTigreBianca38287", L"Principessa9832888",
        L"Cihy_PL", L"Maciek11PL", L"Sarabat92790", L"Camion4493015", L"Krzychhh", L"Haszczak32", L"MorderczaKaszanka", L"FusilliTime", L"Martittaaa95541", L"Arczi80408",
        L"LegolasAmoreMio", L"ilGattopardo7387", L"LaCorsaDellaFoglia", L"HakerBonzo66831", L"Mati9915429", L"Buongiorno Rega", L"MrPatatafrita", L"DarioAvaro", L"SpaghettiSquad25356", L"ProdigioLatino",
        L"Alvaritooooh", L"Gloriainfinita", L"\u0427\u0438\u043F\u043E\u043B\u043B\u0438\u043D\u043E41737", L"OmeletteDuFromage", L"Se\u00F1orFiesta60631", L"Kokore\u00E7_Kafa", L"OrganikFasulye", L"D0mb1l1P4nd4", L"BaguetteWarrior11485", L"FromageSauvage15026",
        L"Usta_Bambi", L"Chef86413", L"MrDupont3090", L"Lapinou65194", L"Asi_Portakal", L"l4hm4cun90563", L"\u0130syanqar21", L"papyonlupenguen19", L"Pembe_\u00D6r\u00FCmcek", L"qelebeqq54",
        L"Scream4Poutine", L"KralKobra12312433", L"ParmesanPeyniri24147", L"Gizmoblock", L"Argathus", L"Chickaboom64324", L"Splut16159", L"Phorgas", L"Wdar71353", L"Thwack51957",
        L"Finreah43721", L"Bronwyn82199", L"A_Juke9", L"Docker13000", L"Emlich1Postich", L"Magnolia62888", L"Dialect4538", L"SaucisseMarteau", L"Greeble49703", L"Elbain89355",
        L"Riinos72451", L"Odile2Ray8510", L"CKiLeLionMtnt", L"Gwen_A_Du", L"Wenawenaaaa", L"Dos-pasitos", L"RumbayCumbia", L"JusteUnBrieDoux28266", L"Gerard2par289828", L"El-duende-Samez87",
        L"Titi_Parisien75561", L"Soytupadre87202", L"Guas\u00F3n-de-la-Noche94", L"Soytuprofe34783", L"PeelyDeservedBetter", L"LeetGamer87", L"Jokeman258", L"MajorYeeter", L"Afuegonski71563", L"SydneyCinderellaSmart",
        L"uwu_T_T", L"MadameIrma", L"LacasadeBangkok", L"GreenGag52", L"Steeeve89002", L"Pescatore_2", L"Nuotare_Via", L"360NoScoper29718", L"Reinedugaming65094", L"VitinhoVarejo",
        L"Lh@amaLhoc@", L"SeguraMeuHeadshot", L"Leoz1n61577", L"Brun01567996", L"M\u00E1nuM\u00E1zinha", L"JoyRJ6420", L"SniperzX18623", L"CapivaraCuritibana", L"RoyaleKing60953", L"AlpacaDaSorte",
        L"Przemek2302", L"JanSerce67", L"TheM\u00F4nia43133", L"SkarpetMistrz", L"DzielnyPacjent_", L"XbonusX70059", L"Morszczyn53033", L"HoraCurka53891", L"Amanda9938643", L"KarpZMilicza",
        L"FanatykKaszanki", L"Bohun15", L"xXxjmarcinxXx", L"TuBrzoza9336", L"TwojStary72546", L"UMnieDziaua", L"Seba129107", L"sebixx6503", L"liroj_drzemkins", L"SmugglerPL",
        L"TrzymamKredens", L"Dziubas324", L"Maczetaaa30552", L"pifpaftrup99843", L"bombeleXXX", L"WujHeniek", L"JasioKlucha", L"szwagier_s\u0105siada", L"Brajanek61598", L"wektorxyz",
        L"RzeliPapom", L"WonszRzeczny81351", L"JestemHardkorem31515", L"zbigniewb89757", L"AlbaThor300", L"AleXandre2k1", L"anton1_51", L"arno_le_gamer", L"Butinator6582", L"CaptainSakapusse",
        L"CarnavalPop", L"Ch0cOpaup", L"chtimarin_2008", L"Destrukteur03", L"ElPitchoune2005", L"Gregbibiche7", L"Hekka_tombe", L"hyperespace184", L"Joelemarsouin", L"Keviiinn075",
        L"KroissantFatal", L"krokmonsieur12", L"LadyMarinaite", L"loukas2803", L"MarlotAzur", L"Max_MouskeTer", L"MoaLaFrite", L"ombre_malfesente", L"Oracle_panda", L"Patapouf_Boss",
        L"PatateCommode", L"paulcastor53", L"Poule_d0", L"ptitbeurre59", L"RafaleNextGen", L"roi_priam_ultra", L"Skelettosaure5", L"Taxxou_82", L"the_1tercepteur", L"Thimonnyer14",
        L"VangeurMaskay", L"xKastagnettEx", L"xXMariAnnieXx", L"Zouki3971", L"\u0635\u0627\u0645\u062F1", L"\u062C\u0627\u06431", L"\u062D\u0645\u064A\u062F21", L"\u0622\u0646\u062B\u0648\u0646\u064A", L"\u0627\u0644\u062C\u0648\u0643\u063185", L"\u0645\u062C\u062A\u0647\u062F1",
        L"\u062C\u0648\u06461", L"\u0631\u0648\u06431", L"\u0644\u0648\u0646\u063A\u0645\u0627\u0646", L"\u0627\u0644\u0639\u0627\u0634\u06422", L"\u0648\u0644\u064A\u0627\u06451", L"\u062A\u0648\u064A\u0633\u062A1", L"\u0631\u064A\u0627\u064685", L"\u0643\u0648\u0646\u0648\u06311", L"\u0643\u0648\u0646\u0631\u0627\u062F", L"\u0641\u0627\u0646\u062A\u0648\u06452",
        L"\u0645\u0627\u0631\u0641\u06441", L"\u062E\u0627\u0641\u064A\u0631", L"\u0645\u0645\u0644\u0648\u06431", L"\u0643\u0627\u064A1", L"\u0643\u0648\u06441", L"\u0622\u0634\u062A\u0648\u0646", L"\u0628\u0627\u063A\u0648\u064A\u0644", L"\u0633\u062A\u064A\u0641\u0627\u0646\u06481", L"\u0646\u0648\u064685", L"\u0623\u0643\u0645\u0644",
        L"\u0633\u0627\u0644\u064524", L"\u0646\u0648\u0644\u0627\u0646", L"\u0643\u0627\u0645\u064A\u0631\u0648\u06461", L"\u0622\u062F\u06451", L"\u062C\u064A\u0645\u06331", L"\u0633\u0639\u0627\u062F\u06291", L"\u0631\u0627\u064A\u062F\u06311", L"\u0641\u0627\u0631\u0633124", L"\u0641\u0647\u06311", L"\u0644\u064A\u062B55",
        L"\u0645\u0647\u0646\u062F124", L"\u0625\u062F1", L"\u0645\u0643\u0645\u0627\u0647\u0648\u0646", L"\u0623\u062B\u064A\u06311", L"K\u00E4sestulle27", L"Z0nK4", L"Helix1_1", L"N0sc0pe1337", L"Krasser_Babo", L"Geckolord1234",
        L"Drachenfeuer63", L"Der_Eismann12", L"MitDemMesser", L"Naibaf981", L"WalterPPK1", L"xX_KopfSchuss_Xx", L"240KMH", L"Erdbeert\u00F6rtchenxD", L"MettwurstMeister12", L"AbraKadaver01",
        L"Mad_Eagle01", L"DickeWumme01", L"_DerBeschte_", L"Waschbrettbauch01", L"Herz-und-Seele", L"V3rnichter", L"Gangsta_Boi01", L"BlaukrautbleibtBlaukraut", L"25_w\u00FCtende_Affen", L"NameWarVergeben2",
        L"Fu\u00DFballfreak2003", L"Maximum1897", L"Jacqueline_die_Queen", L"KingKevin01234", L"Draconis_Sanguinis", L"SchockerZocker01", L"Brummibernd", L"Schali4Lyfe", L"TrinkGenugWasser_", L"IchKannNichLesen",
        L"D\u00F6ner-macht-sch\u00F6ner", L"DerBallermann01", L"ZombieZven", L"Dr\u00F6lf01", L"Bagigio_99", L"UltraMolise", L"HogHog28", L"MilesRomero", L"G3rard0_313", L"Apricot661",
        L"SPAL_2019", L"Fen0Men02", L"Panzabirra98", L"SimoneSpakky", L"Tr0pp0Tr4pp0", L"Bartolone811", L"CGCMcasone", L"Inaffidabile222", L"MangolinoPilo", L"44Sedano44",
        L"Blastan000000bz", L"Fausto771", L"PandinoSuperstar", L"lambrates85", L"MissK921", L"Marasma800", L"Sm4tt0", L"Joker8926", L"404Zurco", L"GiusyLove212",
        L"Frutilla_80", L"Heavygatto", L"MatteBest99", L"CocoMarySRL", L"MisterAngus000", L"Mannara2000", L"Vazkor_AN", L"Arrigoni347", L"MaicolWin1", L"MonkyBanan2002",
        L"AlmaPura95", L"HeavyGionny", L"ClaudiusCamillus", L"mariusz125", L"wektor77", L"panpankracy1", L"xXxwodzuxXx", L"sztygar261", L"mojamama6", L"lepszyodciebie1",
        L"WodnaBestia", L"bezsensu44", L"pestka16", L"NAJLEPSZYW2018", L"M_I_E_T_3_K", L"biceps_1", L"widze_cie_", L"boohay2", L"KowalskiDariusz2", L"lojalnajola1",
        L"zawszewgrze", L"fioletowa22", L"ostrouchy_", L"misiubysiu", L"swobodny6", L"lubiegrzyby1", L"kubecky1", L"superkuba14", L"MistrzDrugiegoPL", L"SuperWygar",
        L"kolektorrr", L"Marchewa3203", L"Schabolub", L"OgniomistrzJan", L"J005ek1", L"PanFarfocel", L"krecik17", L"kostropaty1", L"UFOlubny", L"kajakkrzysztof1",
        L"pierogizkapusto", L"wakacje2019", L"kawanaczczo", L"\u0414\u0430\u0439\u041F\u044F\u0442\u044C", L"\u041F\u0440\u043E100 \u041E\u0442\u043F\u0430\u0434", L"\u0413\u0440\u0438\u0431\u044B\u0421\u0413\u043B\u0430\u0437\u0430\u043C\u0438", L"\u04270\u0442\u043A\u0438\u0439", L"\u0411\u0430\u0442\u043E\u043D1", L"\u0414\u0430\u0432\u0430\u0439\u0414\u043E\u0441\u0432\u0438\u0434\u0430\u043D\u0438\u044F1", L"007\u0412\u0438\u0442\u0435\u043A",
        L"\u041A\u0438\u043B\u044C\u043A\u0430_\u0432_\u0442\u043E\u043C\u0430\u0442\u04351", L"\u0421\u043A\u043E\u0442\u0438\u043A", L"\u041E\u0432\u043E\u0449\u0435\u0431\u0430\u0437\u04302019", L"\u0414\u0438\u043D\u0430\u043C\u0438\u0442\u0425", L"\u041E\u0442\u0434\u0430\u0439_\u0441\u043A\u0435\u0439\u0442", L"1\u0422\u0430\u043A\u043E\u0439", L"9\u042F\u043A\u043E\u0432", L"\u0418\u0432\u0430\u043D \u0421 \u0423\u0441\u0430\u043C\u0438", L"\u041F\u0435\u043B\u044C\u043C\u0435\u043D\u044C1", L"\u0428\u0430\u0432\u0435\u0440\u043C\u0435\u043D",
        L"\u0414\u0440\u0438\u0444\u04422", L"\u041D\u0435\u0434\u043E\u0431\u0440\u043E\u0435\u0443\u0442\u0440\u043E", L"\u041C\u0430\u043A\u04411002", L"\u0421\u0443\u043F\u0435\u0440\u043B\u0430\u0439\u0442", L"\u041A\u0440\u044B\u043B\u044C\u044F\u041C\u0440\u0430\u043A\u0430", L"\u041F\u0438\u0442\u0431\u0443\u043B\u044C1", L"\u0420\u0435\u043B\u044C\u0441\u044B \u0428\u043F\u0430\u043B\u044B", L"\u0412\u043E\u0438\u043D124", L"\u0428\u0435\u0440\u0441\u0442\u044F\u043D\u043E\u0439\u0412\u043E\u043B\u0447\u0430\u0440\u04301", L"\u041F\u0440\u0438\u0432\u0435\u0442\u043F\u043E\u043A\u04301",
        L"\u0412\u043B\u0430\u043435", L"\u0423\u0440\u0430\u043B100", L"\u0421\u0438\u0431\u0438\u0440\u0441\u043A\u0430\u044F \u041A\u043E\u0448\u043A\u0430", L"\u041F\u0438\u0442\u0435\u0440_\u043D\u0435_\u0441\u043F\u0438\u0442", L"\u041A\u043E\u0440\u043E\u0442\u043A\u0438\u0435\u043D\u043E\u0433\u0438", L"\u041F\u0430\u0440\u043A\u0443\u044023", L"\u041B\u043E\u043B\u043A\u0435\u043A3", L"\u0421\u043D\u043E\u0432\u0430_\u0445\u0430\u0439\u043F", L"\u041A\u0435\u043A\u0430\u0440\u044C1", L"\u0428\u0430\u043C\u0430\u043D13",
        L"\u041D\u0430\u043A\u0430\u0437\u0430\u0442\u0435\u043B\u044C555", L"\u0414\u0438\u043C\u0430\u044114", L"\u0412\u0435\u0447\u043D\u044B\u0439 \u041F\u043E\u043B\u0434\u0435\u043D\u044C", L"\u0425\u0414\u0438\u0441\u0441\u0425", L"Kokore\u00E7_Kafa", L"l4hm4cun1", L"OrganikFasulye", L"D0mb1l1P4nd4", L"Usta_Bambi", L"KralKobra1235",
        L"Asi_Portakal", L"\u0130syanqar21", L"ParmesanPeyniri1", L"papyonlupenguen9", L"Pembe_\u00D6r\u00FCmcek", L"qelebeqq54", L"MistikMaymun1", L"elektrogitar_", L"d0nmuss0s1s", L"XxBenEfsaneimxX",
        L"lamakafa1", L"deli_koala", L"PaTaTeS_KaFa3", L"d\u00F6nerci_MeMo", L"b4mb1r31s", L"Sar\u0131_Kavun", L"Diplomas\u0131zDoktor", L"KafadanHeadshot1", L"\u0130n\u015FaatM\u00FChendisi1", L"akl\u0131n\u0131z\u0131_al\u0131r\u0131m",
        L"Katil Kene01", L"Yaramaz Yarasa", L"PaLyANChO", L"\u00C7\u0131t\u0131rs\u0131z Kemik", L"Ay\u0131do\u011Fan_19", L"\u015Eakac\u0131 \u015Eemsiye", L"K\u0131ll\u0131 \u00D6r\u00FCmJack", L"Makasatarrr", L"\u015EahinG\u00F6z_04", L"TamKafadannn",
        L"Muzonun Kabu\u011Fu", L"\u00C7ocuk Madam", L"benidarLAMA", L"NarE\u015Fkisi1", L"PatatinoSalvahe", L"DestruyeBravas", L"disloka01996", L"xXxdark_tonixXx", L"pedrileanguile", L"AngryMiau",
        L"locodelcoco8836", L"asdruval_18", L"pepinovski92", L"se\u00F1orsentin", L"ginga872", L"pimpampum2019", L"crazycharly993", L"burlaosquad99", L"Lallamaquellama2", L"illomenuohersot",
        L"M1st3r0wn3r", L"Rrrrumba98", L"dante_killer13", L"MrPiumPium", L"LordHamburguesa2", L"AplastaBravas", L"El_Pr098", L"BravasHazard", L"ZeroM4nqueke", L"Robak1lls",
        L"Chimuelopr0", L"Pr0K1ller08", L"El_mejor_de_casa", L"pamancotu", L"palomaPalma_1991", L"paprikalost1252", L"jer0loki007", L"voitodiego", L"jositoamnesio", L"XXXluis_98XXX",
        L"MarrulleroHD", L"Bottellonssio89", L"mr_crockersindo", L"brun026", L"leoz1nnn1", L"JoyxxxRJ", L"XsniperzX1", L"royaleking15", L"loco^das^lhamas", L"segura_meu_HS",
        L"M\u00E1nu_M\u00E1zinha", L"capivara16", L"TheM\u00F4nia1", L"cau\u00EA_sp_97", L"juliano\u00CD\u00CD\u00CDBR", L"dani_gato1", L"jogadorsemnome05", L"marc\u00E3oBA_BR", L"Jo\u00E3o_SP", L"Extrupicio3",
        L"semnome0", L"torpekills", L"pAnDoRa10", L"n00bk1ller", L"tligtlig", L"lek_rj_br", L"2jo\u00E3ozinho_bh2", L"carniceir00", L"XXplosiv00", L"topzer@@@",
        L"k1\u00ED\u00ED3R", L"filipinh0sp", L"mr_jonjon", L"zzPAPAICHEGOUzz", L"kt1r0_100", L"biel~30", L"chinela_voadora1", L"`tigredoagreste`", L"&darkBeast&", L"XxdivadaskllxX",
        L"pulgaPG07", L"dead_docks", L"x0baiacubolad0x"
    } },
    { "PhoebeNames_OCE", {
        L"0PingSting", L"12Siblings12", L"2xPump4Life", L"_BigBainPlayz_", L"-=Fr0stDevil=-", L"1eyedInvent0r", L"360walkaway", L"42Wow47", L"2sweaty23", L"0mn1v0r378",
        L"PanicButton47", L"1Forest120", L"8BallWasHere", L"4Regionals", L"90sRunner", L"A Sweaty Dog", L"SpectrumKnight13", L"aaaaaaaaalbert", L"4tnite4ever15", L"aaaaaleeex",
        L"ADudeNamedPhil", L"90cranker7", L"AGenuineHuman", L"ActivateFail", L"aClassyGamer94", L"Agoraphone-8", L"AGoodOmen", L"Agua_Cat", L"Ahat4aMouse", L"AggroHamster59",
        L"Aim Like Idaho", L"Albacore6", L"airplanelegs", L"AligatorPear", L"TheBe$t", L"Allsmyles:D", L"AllTheCandy", L"Archibard11", L"amygdallama", L"AmpleBlindspot",
        L"ALLCAPS67", L"AlphaPlayer34517", L"ArcticAndroid", L"AlwaysOnTime38", L"Abaddon74", L"Angrification", L"Anonymutt", L"Alternative74", L"APeelySmoothie", L"Ambrosia0",
        L"AngryRabbit9", L"HippoMagician", L"AngryRoommate10", L"SweetPenguin16", L"Arclite28", L"CarpMedley", L"SaberPhoenix75", L"AthenaOrApollo", L"AtomicTerr\u221A\u2264r", L"AttackOLantern",
        L"AngryDuck51", L"Atomizer5", L"AyeBall23", L"AngelEye30", L"Primefall44", L"Armstrong78", L"Baethal", L"BabySeahorse81", L"StoneDragon61", L"Back2Work61",
        L"DarkTarget12", L"Bag0Hammers", L"GerbilTrapeze", L"BadKahuna94", L"BananaPastrami", L"Banandana82", L"BattleHammer7", L"ToxicTropic", L"BagelBoy82", L"Balefire81",
        L"JonesyWannabe46", L"Barktooth81", L"Beebitme", L"SlothLife16", L"BeepGoesBeep", L"Represent82", L"BearPuns55", L"Belltop", L"BertIsNice", L"BeeHive7",
        L"BetaChampion", L"BehindYou60", L"BellyFlop40", L"BioDragoon", L"BeefSupreme35", L"BitOJoy", L"BillyBrilliant23", L"BigBuddy15", L"BlinkImGone44", L"Blizzardbeard77",
        L"BlipBlap53", L"BlindCat47", L"Blueberry1002", L"BoatRider10", L"Boomstick59", L"BoatingIsLife", L"Bobtimus", L"BoldPrediction", L"Blackjack31", L"BonyHumans",
        L"Bongodrum010", L"BobsUrUncle71", L"BootifulSnow", L"BotheredBud1", L"BootThisUp256", L"BoxOfGoblins", L"BorisTheGreat37", L"BrainInvader", L"BroSincerely", L"BraddleRoyale80",
        L"Bro2Max", L"BrandNew47", L"BreadSandwich42", L"BeRooted", L"Brotato3131313", L"Brometheus52", L"BossBoi20", L"BreakerBreaker58", L"Bronson66", L"BrokenRecord26",
        L"CryptoDancer", L"BubbleDragons", L"BubblyFalcon", L"Nocturne9964", L"BrutalNoodle91", L"BatTacos", L"Brutalis42", L"Buffering5", L"BumbleB3ar", L"Bundledbats",
        L"BurgerGuy1011", L"PuppyDance9", L"BusinessJoe", L"ButchSquire", L"BuddyGuy9518", L"BushCamper2477", L"butfirstCOFFEE67", L"ButternutQuash", L"BewareTheGOAT", L"ButterKid24",
        L"C-Block54", L"CactusDad80", L"SublimeCactus24", L"CafeDave58", L"CandyPlumber", L"CovertClown95", L"CannotDo42", L"CaptainKiernan", L"CantTouchMe67", L"CantHearYou3",
        L"CarneAsideOf", L"Skyfire85", L"Carcrusher94", L"Caspian79", L"Cathode57", L"Centice49", L"BlackCat61", L"CherubTree", L"Channel2659", L"Ch0pp3r66",
        L"CharmedImSure85", L"ChestOfFault", L"Cannonball86", L"Primetime71", L"Cheese10128", L"iHazHighGround", L"ChickenFarmer17", L"Lexington90", L"ChiliDog87", L"ChillMono",
        L"Chorizo1337", L"Chronicled5", L"Chug_Brigade", L"ChArter52", L"Chronolink24", L"FootballDay", L"ColonelChugJug", L"CleverPanda91", L"Chipper70", L"CleverPun92",
        L"Cleopatra30", L"CoffeeFueled84", L"ConstantThorn", L"CounterMantra", L"ColonelFox41", L"ComingForYou19", L"CopperBottom43", L"Corrosion86", L"CrabInMyShoe", L"CowJuice92",
        L"CrepeSalad", L"CrazyJet83", L"CopyKat58", L"CrazyPea96", L"CrocodileBard", L"CruiseOn", L"CrabbyBaby71", L"Crestfallen64", L"Crypt1c60", L"CtrlAltDeleted94",
        L"CyborgMelon", L"CrumbBum91", L"Curveball20", L"d0m3dw3ll3r", L"N0nDa1ry", L"CyberWolf26", L"DanglingKeys", L"DarlingSnail", L"D3m0nk3y56", L"Dawson201",
        L"Damarus83", L"DanIsHere50", L"DeepStealth", L"DefiantGerbil", L"DisIsMatt", L"DekesNdangles", L"2Xtrouble13", L"DemonOnWheels", L"BraunyBanana", L"Dinosaur4Hire",
        L"DJHoneybun", L"Dessloch52", L"Dimitra66", L"DiamondQuartz78", L"Disconnect21", L"Divisor78", L"Caveman59", L"DoctorGungHo", L"DrQuikMaths", L"PopNLock7",
        L"Doilies4Days", L"DoctorLobby92", L"Strider0728", L"DoublePinpoint", L"BestDoggo83", L"DoubleDaring", L"DoubleBlitz52", L"DoubleDuel75", L"DoublePumpkin78", L"DoubleRainbow96",
        L"DrPlanet", L"DoUeven84", L"Dreamdunker", L"DramaAlert5", L"Doyourdishes33", L"Drekorig97", L"DJLightning85", L"Drimacus7", L"DrPlants40", L"Dunkball96",
        L"DeluxeDino18", L"DreamDog18", L"EarOfCorn1010", L"Eggwithnosalt", L"EdgeGoose", L"HyenaBrigade", L"GoldenOctopus0", L"LazyLlama7", L"KeyboardKing32", L"JimmyIsOnFire97",
        L"SadTroll94", L"Elftastic", L"Erraatic27", L"Eubanker555", L"ElderFly13", L"Exhale34", L"EndlessMike52", L"F1veFruits", L"EvilJonesy12", L"Fall3nSlasher",
        L"ElPollo85", L"FacelessPanda56", L"FauxFeathers", L"FallingInStyle86", L"FancyFingers87", L"FantasyCruz98", L"PositiveFeels", L"Faraleth90", L"Fictionpoint", L"FavoriteFlavor45",
        L"FightinFishBoy", L"FinlessShark", L"FeasibleWeasel80", L"TimeSplat", L"FishingEvryDay", L"FireCheeks56", L"FireDynasty68", L"5HoleSniper", L"FlamingoGuru", L"FishWithLungs27",
        L"FlankTheTank", L"FlavorCapitan", L"SomethingFishy3", L"FlawlessGame", L"FluffySardine", L"Flyingfender", L"FootBarn", L"Firewall4", L"FlossPatrol82", L"Forge62018",
        L"ForksNSporks", L"ForlornMorning", L"FluffyMuffin8", L"ForsakenVoyage", L"Forbidden60", L"ForkAndKnife17", L"ForkSpoonKnife42", L"Fortifiable", L"FreeCookies", L"FortGoodNite43",
        L"FortniteKid34558", L"Frediculous20", L"FreeMinded49", L"Freylis21", L"FriendlySniper27", L"FrogSpawn42", L"FunkLogic17", L"FunkyFresh1349", L"Frostbitten86", L"FuzzyPickl3s",
        L"g0tmy3Y3onU", L"FuriousBug50", L"GalaxyBrain77", L"GamerGreat30", L"GameWinnerz87", L"Gaargod55", L"Glamour67", L"FunSized86", L"Freefire63", L"GeekTragedy",
        L"GeoForts", L"GiantDaisy", L"Goosezilla13", L"Gatordile81", L"GiaPuppy", L"GhostColour6", L"GigaWatt10k", L"GimmeCrisps", L"GimmeMoar", L"FancyPumpkin87",
        L"GlassCannister", L"GlitterMantis", L"GhostChicken12", L"Giuseppina71", L"Gkublok13", L"GloatingEmu", L"Glopazoid", L"GlowBeetle41", L"GnomeOverlord", L"FlimsyGoat",
        L"GoblinSelfie", L"GnomeRider16", L"GoldenCardamom", L"Goldie971", L"GoldenMage12", L"GoldenManatee6", L"GolfVoid", L"Going4Gold17", L"GoneFishin15", L"GardenDolphin",
        L"GradedOnACurve", L"GotHeartburn", L"gonnagetcha15", L"Gooddoggo80", L"Gigabyte51", L"GreasyGoal82", L"Greedy33", L"Initialize0", L"Dominator25", L"Gridl0cked",
        L"Grandma40", L"GrannyGunner3", L"GreatGriffin28", L"GoodDawg13", L"GrumbleofPugs6", L"GrimRanger72", L"Guacamoldy7", L"GuardianHeart4", L"GuessWhat23", L"Gurshwunner",
        L"H1pC0c0nut", L"HaaalpMe", L"GunGoesPewPew38", L"HangryBuddy", L"HamsterTaxi", L"Hydrohawk7", L"HaHaHeeHee73", L"HalfEatenPizza82", L"HandsomeLemon34", L"HashTagonistic",
        L"HauntedDreams", L"HappyMelon50", L"HazardousPaste", L"HashtagToad57", L"HeliumHog", L"Here4Dancing", L"HawkWrath74", L"HermitCrab26", L"Hevymeddle", L"PlzBeNiceToMe",
        L"HopOnDown20", L"HelloMyDude54", L"Hashbrown31", L"HollowBrick5", L"HeyThereFriend81", L"MajesticMutant", L"HeroicHealer71", L"Hoodwinked12", L"Hiveminder97", L"HotelBlankets",
        L"HowAreMy90s", L"Horse4227", L"HubcapDonuts", L"SpringChicken10", L"ISeeCiabatta", L"Howitzer98", L"iAteMySpacebar", L"IAmW4t3r", L"IronHype58", L"HomeSkillet95",
        L"Hollywood70", L"IckyWombat", L"ifyougotem", L"IGottaGoToBed", L"IceFork43", L"Ichthys25", L"IHaveFullBrick", L"IGotSticks85", L"IhaveAGoodElo", L"IHaveFullMetal",
        L"IHateBacon35", L"ImGonnaWin2234", L"InflatedCouch", L"InsideAgent", L"LetsBePals23", L"ImFam0us24", L"InfinityPigeon66", L"ImReallyADog76", L"IntoTheOcean", L"InverseUnicorn95",
        L"IronBoss86", L"IrritatedIris", L"IsBruceHome", L"InnoScent24", L"IronMade20", L"Irontide57", L"SalsaVerde50", L"SilentRogue98", L"IvanDude0001", L"IsOnlyGame12",
        L"IamPizza5", L"BillysMom17", L"ElectricKitten12", L"MarathonWalker", L"LostYeti57", L"MagicJackie8", L"JebidiahSparks", L"VeryMuchGG", L"TallestAnt4", L"JewelOfWisdom",
        L"JellyBeats81", L"JollyGoat46", L"JustSugar85", L"JonnyTheKid52", L"JonesyForever35", L"ImmortalMango66", L"Happy2Bhere39", L"JumpyMcJump64", L"JackalViper", L"JustBuster73",
        L"TermiteMuscles", L"KenCrunch", L"KeyFlamingo", L"VerticalSlice", L"Juxtapose48", L"KatKat200239", L"WigglyCow", L"KingFlexor", L"SimplyKraken", L"KeepScrolling95",
        L"KingOfFudge", L"ThroneBlade47", L"KimchiFighter13", L"BoogieKing85", L"Knowaguy", L"KingToucan2", L"KingKoala82", L"FickleKnight20", L"DoomCrow74", L"ColorfulCoyote",
        L"Kragoth93", L"Quack4Bread", L"Kregore73", L"Shadow74739", L"LambFury", L"LavaHound68", L"JungleProwler25", L"1337Sk1lz", L"Landsnail80", L"LarsTheKid25",
        L"Laserfrog44", L"ManiacMeerkat49", L"JustABitEpic", L"LewtGoblin7", L"Liftoff36", L"Lightsp33d79", L"LilBandit94", L"LilGrande56", L"LittlePidgeon", L"LaidBackBeats",
        L"LousyCentaur", L"LovelyLemur", L"LobbyBound43", L"LilMeowMeow50", L"DangerLeopard", L"LootTrooper51", L"Mayonnaise48", L"LadyLuck75", L"Lulabyebye", L"LuckyNumber4279",
        L"LuckyLadybug65", L"MagicQuail", L"MaximumMad53", L"Lizardfish49", L"MajorProdigy51", L"majorOOF49", L"MadBrad50", L"Makreth4", L"Mandible13", L"Manwell65",
        L"MarineDog54", L"Malakai96", L"MaximumBro3", L"MaximusIon59", L"MaybeNot40", L"MeanSardine", L"KittyCat80", L"McCucumber71", L"McLawnmower91", L"MegaYeet98",
        L"MemoryGobbler", L"Megacrash21", L"Meshuncle", L"MeLlamoJefe76", L"Meshdaddy85", L"MightyMonkfish", L"MightySobek", L"MidnightScar26", L"Memphis31", L"Marionette84",
        L"Thx4DaLoot14", L"MeowMeow38", L"MissedJumpshot", L"Mission1536", L"MintElephant26", L"RareOrange", L"MisterTaboo", L"MisterMosquito0", L"MongLason", L"MooseRampage",
        L"UltraMom83", L"MondayMourning82", L"MoreOfTheSame", L"MoonWizard76", L"MsFroggy123", L"Monolith95", L"Mouthful95", L"SpeedSparrow47", L"MrBurgers39", L"MayorMVP",
        L"MuddyBoots7", L"Nanochunks", L"JokeFountain", L"SwampRhino", L"MutantGecko7", L"NavyQuail", L"Nashtyboy67", L"NeoDynamic", L"Fearmaster62", L"Mountain83",
        L"NeonSalmon", L"NegativeNight7", L"NocturnePatrol", L"NomnomCarver", L"MysteryMan79", L"NeoHero4", L"NonCarbon8", L"ZeroOverDrive", L"ElegantBeast39", L"TooManyBeets",
        L"NotMyLasagna18", L"Numbers789", L"Nickolai62", L"NoodleSoup83", L"Number141", L"OblongWidget", L"NumeroDos28", L"OddComplex85", L"OldWaterBottle28", L"OleReliable78",
        L"OneMillion17", L"OneGlimpse87", L"OnlyMust4rd", L"OneMillionAnts24", L"OpenShutCase", L"OpenSeason23", L"OrangeTango33", L"Overtime46", L"TroubledTaco45", L"9000Hippos",
        L"Ouboudah25", L"OxfordCommas", L"ItsAlwaysMe", L"Othello54", L"Ophelia66", L"Paracord", L"PopcornSalt3", L"PapaParadise82", L"ParanoidCactus", L"PebbleSpider",
        L"NotAPalindrome38", L"PapaJimbo9", L"PepperTaxi", L"PeskyInfant", L"PickaxeUAQ", L"FrogTheCat18", L"Smoothie66", L"PizzaUnic0rn", L"PerpleXed79", L"Pinnacle6",
        L"PlatinumPony", L"PirateWizard22", L"PlayOnRepeat", L"Playmaker365", L"PlasticBullets62", L"PoachedPepper", L"PortableOx", L"PortaSniper", L"PLZST0P75", L"pleaseletmewin10",
        L"PotatoCupid", L"ViciousPie44", L"PowerFlank", L"PrancingPwnee", L"PrimeSystem", L"PrinceWombat", L"PrincessImp", L"PotsNPans39", L"Prisoner19A", L"PsyonicOx",
        L"PurpleRocks99", L"QuartetUltra", L"QuantumFig", L"PumpkinKnight83", L"PureSneak92", L"QuadAlex13", L"QWERTYb0ss", L"R00kMine6", L"RabbitKicks", L"PurpleCrayon85",
        L"R0ckStar5", L"Ramsbottle", L"RainbowZebra36", L"RandoDog73", L"Rainmaker67", L"Ramp4ge71", L"RavensRevenge13", L"RatherBFishin0", L"RealMisfortune", L"RelaxOkay",
        L"Reach4TheSky54", L"RedOrange2240", L"Rhett5408", L"RiskyDigits", L"RagePower81", L"Riftmage54", L"Riboflavin52", L"RoarTiger24", L"GoodRobin6", L"RoyaleCorral",
        L"RocketBoi87", L"Rocking7614", L"RudeAwaken1ngz", L"Roundhouse47", L"SalemSquid", L"RustyShine93", L"SLUGshot65", L"Sabretooth67", L"Salamander8910", L"Sandra6000",
        L"SandSpider2000", L"SalsaPants36", L"SaltySandwich81", L"SappyPirate", L"Sandwitch90", L"SassyJazzhands", L"ScorpionOne50", L"UnderwaterSpy77", L"SeagullTamer", L"BitterSecret",
        L"SeeUFriday", L"Gearhead46", L"SergeantSummer", L"SeriousSteve35", L"Sevarog3", L"Seven8Nine93", L"ShadowArrow58", L"Sapphire59", L"SilverBear89", L"BubbleHunter22",
        L"ShieldHorse63", L"Shepard52", L"SwampJackal", L"ShiniestBovine", L"ShogunGiraffe", L"Brainshocked0", L"Shoot2Thrill25", L"Shotdrop87", L"ShootyMcGee40", L"ShoTTy4Life48",
        L"Sharkboy61", L"Shotty2thebody66", L"ShottyBOI1", L"SickPapaya", L"ShrimpHeaven74", L"Shrimpz62", L"SilverySliver", L"SilentBanana17", L"SimplyTheBest97", L"SirChimera83",
        L"SirTricksALot21", L"SizzlinChamp", L"SixesAndSevens65", L"SkeletonMonkey14", L"Slammie9", L"sleepingmantis47", L"SlickElephant", L"SleepyPete56", L"SMGsRdaBest", L"Smellwonk",
        L"Pickles60", L"SlurpItUp93", L"Sn00tyMagician", L"SneakyBush9898", L"SniperCharlie92", L"JakeTheSnake7770", L"SnipersDelight50", L"Reaper1487", L"SmittenKitten4", L"GetScoped92",
        L"SoFastMan", L"Sniper36094", L"Snowday95", L"Solocamper8", L"SoggyCookie26", L"Renegade53", L"SoloSpy86", L"SonicComet37", L"SoulIntellect", L"SoSweaty63",
        L"SoulSpecter", L"SpaceRivers", L"BridgeBuilder87", L"Soundthealarm18", L"SpaceChimp18", L"SpaceLight13", L"SpaghettiCode77", L"SpiderMitten", L"SpokenWard", L"SpookyLeader21",
        L"SpeedyTheCat98", L"SquadMaster22", L"StaleGrapes", L"SporkMaster20", L"StarryGolds", L"SquadUp98", L"Squar3R00t5", L"AtTheBeach321", L"StarWarden80", L"StayHydrated41",
        L"SteelGoose18", L"StoicAlmond", L"TheFreezer21", L"StickyFingers60", L"Sugartech", L"StrangePlant87", L"StringCheese15", L"Sundown57", L"Subversa39", L"GoodIRL",
        L"SuperPoshGrape", L"SuperCandyGurl77", L"Syrupmancer", L"TacoArmada", L"SuperDogCat43", L"SurfRider10146", L"Swabbie23", L"TacticalFish64", L"TacoHands33", L"TAgYOuRIt9",
        L"TaterTots6789", L"Tamerlan30", L"Telepwn78", L"TearCollector18", L"Telltool", L"TankBro85", L"M0arMinutes", L"TepidPorridge", L"FroyoLicker", L"ThatHURTS30",
        L"DangerZero32", L"Thannis80", L"QueenBeet74", L"CatsClause", L"TheBestThereIs45", L"Tacocat60", L"FenderBlender7", L"TheFlopperKing", L"MorninWaffles", L"FishermanJoe86",
        L"MyNameIsHelga15", L"SlurpMaster650", L"OnetrickWonder64", L"Stinger85", L"WindedCrusader", L"Monkeyking17", L"ThreeJabLad", L"NullAndVoid58", L"Notatest22", L"ThermalDragon39",
        L"ThisSideUp87", L"GunShowTickets", L"TigerDreads", L"ThunderKarma33", L"WowerZower", L"TinyMcLarge", L"TickTickBoom57", L"Tigertooth91", L"Time2Go80", L"TLDR4life",
        L"TigerSmash3", L"TinyMoose50", L"DrumGunnar", L"TopDanger28", L"TotallyJeff", L"TreesapBetty", L"BestRegards81", L"Triangulated86", L"TryAgain49", L"FreshFish71",
        L"TurtleUp12", L"TwoHitWonder97", L"TwoWeeks18", L"UndeadBadger", L"UltimateSteve26", L"Valley6789", L"UltraDavid26", L"Untouchable92", L"Mascot2458", L"Vectron29",
        L"PureBacon94", L"VeggieBurger24", L"WaffleDoctor", L"VisionRunner18", L"WanderAgent", L"CyborgWalrus6", L"WalkInThePark66", L"WildfireWizard", L"WalkTheDog80", L"Vertigo70",
        L"Warning1496", L"WildCactusBob", L"Warsiren14", L"Whitman40", L"WeekendWarrior81", L"WickedWasp10", L"Wildonkey", L"Wondertail", L"WrathofKnight", L"Wolfman11",
        L"WolfPhantom46", L"WiggleShark28", L"XenoBotanist27", L"XxBonemealxX", L"xX360NoScopeXx28", L"xXAnonymousXx30", L"Yarley0001", L"HoorayBeets", L"XxCoolNamexX63", L"XxR3AP3RxX64",
        L"YoureNotYour", L"YeetLife35", L"YellowSauce80", L"YourBestFriend63", L"UltimateZenith14", L"Bubbles41", L"Yeetman57", L"Scorpio62"
    } },
    { "PhoebeNames_BR", {
        L"0PingSting", L"12Siblings12", L"2xPump4Life", L"_BigBainPlayz_", L"-=Fr0stDevil=-", L"1eyedInvent0r", L"360walkaway", L"42Wow47", L"2sweaty23", L"0mn1v0r378",
        L"PanicButton47", L"1Forest120", L"8BallWasHere", L"4Regionals", L"90sRunner", L"A Sweaty Dog", L"SpectrumKnight13", L"aaaaaaaaalbert", L"4tnite4ever15", L"aaaaaleeex",
        L"ADudeNamedPhil", L"90cranker7", L"AGenuineHuman", L"ActivateFail", L"aClassyGamer94", L"Agoraphone-8", L"AGoodOmen", L"Agua_Cat", L"Ahat4aMouse", L"AggroHamster59",
        L"Aim Like Idaho", L"Albacore6", L"airplanelegs", L"AligatorPear", L"TheBe$t", L"Allsmyles:D", L"AllTheCandy", L"Archibard11", L"amygdallama", L"AmpleBlindspot",
        L"ALLCAPS67", L"AlphaPlayer34517", L"ArcticAndroid", L"AlwaysOnTime38", L"Abaddon74", L"Angrification", L"Anonymutt", L"Alternative74", L"APeelySmoothie", L"Ambrosia0",
        L"AngryRabbit9", L"HippoMagician", L"AngryRoommate10", L"SweetPenguin16", L"Arclite28", L"CarpMedley", L"SaberPhoenix75", L"AthenaOrApollo", L"AtomicTerr\u221A\u2264r", L"AttackOLantern",
        L"AngryDuck51", L"Atomizer5", L"AyeBall23", L"AngelEye30", L"Primefall44", L"Armstrong78", L"Baethal", L"BabySeahorse81", L"StoneDragon61", L"Back2Work61",
        L"DarkTarget12", L"Bag0Hammers", L"GerbilTrapeze", L"BadKahuna94", L"BananaPastrami", L"Banandana82", L"BattleHammer7", L"ToxicTropic", L"BagelBoy82", L"Balefire81",
        L"JonesyWannabe46", L"Barktooth81", L"Beebitme", L"SlothLife16", L"BeepGoesBeep", L"Represent82", L"BearPuns55", L"Belltop", L"BertIsNice", L"BeeHive7",
        L"BetaChampion", L"BehindYou60", L"BellyFlop40", L"BioDragoon", L"BeefSupreme35", L"BitOJoy", L"BillyBrilliant23", L"BigBuddy15", L"BlinkImGone44", L"Blizzardbeard77",
        L"BlipBlap53", L"BlindCat47", L"Blueberry1002", L"BoatRider10", L"Boomstick59", L"BoatingIsLife", L"Bobtimus", L"BoldPrediction", L"Blackjack31", L"BonyHumans",
        L"Bongodrum010", L"BobsUrUncle71", L"BootifulSnow", L"BotheredBud1", L"BootThisUp256", L"BoxOfGoblins", L"BorisTheGreat37", L"BrainInvader", L"BroSincerely", L"BraddleRoyale80",
        L"Bro2Max", L"BrandNew47", L"BreadSandwich42", L"BeRooted", L"Brotato3131313", L"Brometheus52", L"BossBoi20", L"BreakerBreaker58", L"Bronson66", L"BrokenRecord26",
        L"CryptoDancer", L"BubbleDragons", L"BubblyFalcon", L"Nocturne9964", L"BrutalNoodle91", L"BatTacos", L"Brutalis42", L"Buffering5", L"BumbleB3ar", L"Bundledbats",
        L"BurgerGuy1011", L"PuppyDance9", L"BusinessJoe", L"ButchSquire", L"BuddyGuy9518", L"BushCamper2477", L"butfirstCOFFEE67", L"ButternutQuash", L"BewareTheGOAT", L"ButterKid24",
        L"C-Block54", L"CactusDad80", L"SublimeCactus24", L"CafeDave58", L"CandyPlumber", L"CovertClown95", L"CannotDo42", L"CaptainKiernan", L"CantTouchMe67", L"CantHearYou3",
        L"CarneAsideOf", L"Skyfire85", L"Carcrusher94", L"Caspian79", L"Cathode57", L"Centice49", L"BlackCat61", L"CherubTree", L"Channel2659", L"Ch0pp3r66",
        L"CharmedImSure85", L"ChestOfFault", L"Cannonball86", L"Primetime71", L"Cheese10128", L"iHazHighGround", L"ChickenFarmer17", L"Lexington90", L"ChiliDog87", L"ChillMono",
        L"Chorizo1337", L"Chronicled5", L"Chug_Brigade", L"ChArter52", L"Chronolink24", L"FootballDay", L"ColonelChugJug", L"CleverPanda91", L"Chipper70", L"CleverPun92",
        L"Cleopatra30", L"CoffeeFueled84", L"ConstantThorn", L"CounterMantra", L"ColonelFox41", L"ComingForYou19", L"CopperBottom43", L"Corrosion86", L"CrabInMyShoe", L"CowJuice92",
        L"CrepeSalad", L"CrazyJet83", L"CopyKat58", L"CrazyPea96", L"CrocodileBard", L"CruiseOn", L"CrabbyBaby71", L"Crestfallen64", L"Crypt1c60", L"CtrlAltDeleted94",
        L"CyborgMelon", L"CrumbBum91", L"Curveball20", L"d0m3dw3ll3r", L"N0nDa1ry", L"CyberWolf26", L"DanglingKeys", L"DarlingSnail", L"D3m0nk3y56", L"Dawson201",
        L"Damarus83", L"DanIsHere50", L"DeepStealth", L"DefiantGerbil", L"DisIsMatt", L"DekesNdangles", L"2Xtrouble13", L"DemonOnWheels", L"BraunyBanana", L"Dinosaur4Hire",
        L"DJHoneybun", L"Dessloch52", L"Dimitra66", L"DiamondQuartz78", L"Disconnect21", L"Divisor78", L"Caveman59", L"DoctorGungHo", L"DrQuikMaths", L"PopNLock7",
        L"Doilies4Days", L"DoctorLobby92", L"Strider0728", L"DoublePinpoint", L"BestDoggo83", L"DoubleDaring", L"DoubleBlitz52", L"DoubleDuel75", L"DoublePumpkin78", L"DoubleRainbow96",
        L"DrPlanet", L"DoUeven84", L"Dreamdunker", L"DramaAlert5", L"Doyourdishes33", L"Drekorig97", L"DJLightning85", L"Drimacus7", L"DrPlants40", L"Dunkball96",
        L"DeluxeDino18", L"DreamDog18", L"EarOfCorn1010", L"Eggwithnosalt", L"EdgeGoose", L"HyenaBrigade", L"GoldenOctopus0", L"LazyLlama7", L"KeyboardKing32", L"JimmyIsOnFire97",
        L"SadTroll94", L"Elftastic", L"Erraatic27", L"Eubanker555", L"ElderFly13", L"Exhale34", L"EndlessMike52", L"F1veFruits", L"EvilJonesy12", L"Fall3nSlasher",
        L"ElPollo85", L"FacelessPanda56", L"FauxFeathers", L"FallingInStyle86", L"FancyFingers87", L"FantasyCruz98", L"PositiveFeels", L"Faraleth90", L"Fictionpoint", L"FavoriteFlavor45",
        L"FightinFishBoy", L"FinlessShark", L"FeasibleWeasel80", L"TimeSplat", L"FishingEvryDay", L"FireCheeks56", L"FireDynasty68", L"5HoleSniper", L"FlamingoGuru", L"FishWithLungs27",
        L"FlankTheTank", L"FlavorCapitan", L"SomethingFishy3", L"FlawlessGame", L"FluffySardine", L"Flyingfender", L"FootBarn", L"Firewall4", L"FlossPatrol82", L"Forge62018",
        L"ForksNSporks", L"ForlornMorning", L"FluffyMuffin8", L"ForsakenVoyage", L"Forbidden60", L"ForkAndKnife17", L"ForkSpoonKnife42", L"Fortifiable", L"FreeCookies", L"FortGoodNite43",
        L"FortniteKid34558", L"Frediculous20", L"FreeMinded49", L"Freylis21", L"FriendlySniper27", L"FrogSpawn42", L"FunkLogic17", L"FunkyFresh1349", L"Frostbitten86", L"FuzzyPickl3s",
        L"g0tmy3Y3onU", L"FuriousBug50", L"GalaxyBrain77", L"GamerGreat30", L"GameWinnerz87", L"Gaargod55", L"Glamour67", L"FunSized86", L"Freefire63", L"GeekTragedy",
        L"GeoForts", L"GiantDaisy", L"Goosezilla13", L"Gatordile81", L"GiaPuppy", L"GhostColour6", L"GigaWatt10k", L"GimmeCrisps", L"GimmeMoar", L"FancyPumpkin87",
        L"GlassCannister", L"GlitterMantis", L"GhostChicken12", L"Giuseppina71", L"Gkublok13", L"GloatingEmu", L"Glopazoid", L"GlowBeetle41", L"GnomeOverlord", L"FlimsyGoat",
        L"GoblinSelfie", L"GnomeRider16", L"GoldenCardamom", L"Goldie971", L"GoldenMage12", L"GoldenManatee6", L"GolfVoid", L"Going4Gold17", L"GoneFishin15", L"GardenDolphin",
        L"GradedOnACurve", L"GotHeartburn", L"gonnagetcha15", L"Gooddoggo80", L"Gigabyte51", L"GreasyGoal82", L"Greedy33", L"Initialize0", L"Dominator25", L"Gridl0cked",
        L"Grandma40", L"GrannyGunner3", L"GreatGriffin28", L"GoodDawg13", L"GrumbleofPugs6", L"GrimRanger72", L"Guacamoldy7", L"GuardianHeart4", L"GuessWhat23", L"Gurshwunner",
        L"H1pC0c0nut", L"HaaalpMe", L"GunGoesPewPew38", L"HangryBuddy", L"HamsterTaxi", L"Hydrohawk7", L"HaHaHeeHee73", L"HalfEatenPizza82", L"HandsomeLemon34", L"HashTagonistic",
        L"HauntedDreams", L"HappyMelon50", L"HazardousPaste", L"HashtagToad57", L"HeliumHog", L"Here4Dancing", L"HawkWrath74", L"HermitCrab26", L"Hevymeddle", L"PlzBeNiceToMe",
        L"HopOnDown20", L"HelloMyDude54", L"Hashbrown31", L"HollowBrick5", L"HeyThereFriend81", L"MajesticMutant", L"HeroicHealer71", L"Hoodwinked12", L"Hiveminder97", L"HotelBlankets",
        L"HowAreMy90s", L"Horse4227", L"HubcapDonuts", L"SpringChicken10", L"ISeeCiabatta", L"Howitzer98", L"iAteMySpacebar", L"IAmW4t3r", L"IronHype58", L"HomeSkillet95",
        L"Hollywood70", L"IckyWombat", L"ifyougotem", L"IGottaGoToBed", L"IceFork43", L"Ichthys25", L"IHaveFullBrick", L"IGotSticks85", L"IhaveAGoodElo", L"IHaveFullMetal",
        L"IHateBacon35", L"ImGonnaWin2234", L"InflatedCouch", L"InsideAgent", L"LetsBePals23", L"ImFam0us24", L"InfinityPigeon66", L"ImReallyADog76", L"IntoTheOcean", L"InverseUnicorn95",
        L"IronBoss86", L"IrritatedIris", L"IsBruceHome", L"InnoScent24", L"IronMade20", L"Irontide57", L"SalsaVerde50", L"SilentRogue98", L"IvanDude0001", L"IsOnlyGame12",
        L"IamPizza5", L"BillysMom17", L"ElectricKitten12", L"MarathonWalker", L"LostYeti57", L"MagicJackie8", L"JebidiahSparks", L"VeryMuchGG", L"TallestAnt4", L"JewelOfWisdom",
        L"JellyBeats81", L"JollyGoat46", L"JustSugar85", L"JonnyTheKid52", L"JonesyForever35", L"ImmortalMango66", L"Happy2Bhere39", L"JumpyMcJump64", L"JackalViper", L"JustBuster73",
        L"TermiteMuscles", L"KenCrunch", L"KeyFlamingo", L"VerticalSlice", L"Juxtapose48", L"KatKat200239", L"WigglyCow", L"KingFlexor", L"SimplyKraken", L"KeepScrolling95",
        L"KingOfFudge", L"ThroneBlade47", L"KimchiFighter13", L"BoogieKing85", L"Knowaguy", L"KingToucan2", L"KingKoala82", L"FickleKnight20", L"DoomCrow74", L"ColorfulCoyote",
        L"Kragoth93", L"Quack4Bread", L"Kregore73", L"Shadow74739", L"LambFury", L"LavaHound68", L"JungleProwler25", L"1337Sk1lz", L"Landsnail80", L"LarsTheKid25",
        L"Laserfrog44", L"ManiacMeerkat49", L"JustABitEpic", L"LewtGoblin7", L"Liftoff36", L"Lightsp33d79", L"LilBandit94", L"LilGrande56", L"LittlePidgeon", L"LaidBackBeats",
        L"LousyCentaur", L"LovelyLemur", L"LobbyBound43", L"LilMeowMeow50", L"DangerLeopard", L"LootTrooper51", L"Mayonnaise48", L"LadyLuck75", L"Lulabyebye", L"LuckyNumber4279",
        L"LuckyLadybug65", L"MagicQuail", L"MaximumMad53", L"Lizardfish49", L"MajorProdigy51", L"majorOOF49", L"MadBrad50", L"Makreth4", L"Mandible13", L"Manwell65",
        L"MarineDog54", L"Malakai96", L"MaximumBro3", L"MaximusIon59", L"MaybeNot40", L"MeanSardine", L"KittyCat80", L"McCucumber71", L"McLawnmower91", L"MegaYeet98",
        L"MemoryGobbler", L"Megacrash21", L"Meshuncle", L"MeLlamoJefe76", L"Meshdaddy85", L"MightyMonkfish", L"MightySobek", L"MidnightScar26", L"Memphis31", L"Marionette84",
        L"Thx4DaLoot14", L"MeowMeow38", L"MissedJumpshot", L"Mission1536", L"MintElephant26", L"RareOrange", L"MisterTaboo", L"MisterMosquito0", L"MongLason", L"MooseRampage",
        L"UltraMom83", L"MondayMourning82", L"MoreOfTheSame", L"MoonWizard76", L"MsFroggy123", L"Monolith95", L"Mouthful95", L"SpeedSparrow47", L"MrBurgers39", L"MayorMVP",
        L"MuddyBoots7", L"Nanochunks", L"JokeFountain", L"SwampRhino", L"MutantGecko7", L"NavyQuail", L"Nashtyboy67", L"NeoDynamic", L"Fearmaster62", L"Mountain83",
        L"NeonSalmon", L"NegativeNight7", L"NocturnePatrol", L"NomnomCarver", L"MysteryMan79", L"NeoHero4", L"NonCarbon8", L"ZeroOverDrive", L"ElegantBeast39", L"TooManyBeets",
        L"NotMyLasagna18", L"Numbers789", L"Nickolai62", L"NoodleSoup83", L"Number141", L"OblongWidget", L"NumeroDos28", L"OddComplex85", L"OldWaterBottle28", L"OleReliable78",
        L"OneMillion17", L"OneGlimpse87", L"OnlyMust4rd", L"OneMillionAnts24", L"OpenShutCase", L"OpenSeason23", L"OrangeTango33", L"Overtime46", L"TroubledTaco45", L"9000Hippos",
        L"Ouboudah25", L"OxfordCommas", L"ItsAlwaysMe", L"Othello54", L"Ophelia66", L"Paracord", L"PopcornSalt3", L"PapaParadise82", L"ParanoidCactus", L"PebbleSpider",
        L"NotAPalindrome38", L"PapaJimbo9", L"PepperTaxi", L"PeskyInfant", L"PickaxeUAQ", L"FrogTheCat18", L"Smoothie66", L"PizzaUnic0rn", L"PerpleXed79", L"Pinnacle6",
        L"PlatinumPony", L"PirateWizard22", L"PlayOnRepeat", L"Playmaker365", L"PlasticBullets62", L"PoachedPepper", L"PortableOx", L"PortaSniper", L"PLZST0P75", L"pleaseletmewin10",
        L"PotatoCupid", L"ViciousPie44", L"PowerFlank", L"PrancingPwnee", L"PrimeSystem", L"PrinceWombat", L"PrincessImp", L"PotsNPans39", L"Prisoner19A", L"PsyonicOx",
        L"PurpleRocks99", L"QuartetUltra", L"QuantumFig", L"PumpkinKnight83", L"PureSneak92", L"QuadAlex13", L"QWERTYb0ss", L"R00kMine6", L"RabbitKicks", L"PurpleCrayon85",
        L"R0ckStar5", L"Ramsbottle", L"RainbowZebra36", L"RandoDog73", L"Rainmaker67", L"Ramp4ge71", L"RavensRevenge13", L"RatherBFishin0", L"RealMisfortune", L"RelaxOkay",
        L"Reach4TheSky54", L"RedOrange2240", L"Rhett5408", L"RiskyDigits", L"RagePower81", L"Riftmage54", L"Riboflavin52", L"RoarTiger24", L"GoodRobin6", L"RoyaleCorral",
        L"RocketBoi87", L"Rocking7614", L"RudeAwaken1ngz", L"Roundhouse47", L"SalemSquid", L"RustyShine93", L"SLUGshot65", L"Sabretooth67", L"Salamander8910", L"Sandra6000",
        L"SandSpider2000", L"SalsaPants36", L"SaltySandwich81", L"SappyPirate", L"Sandwitch90", L"SassyJazzhands", L"ScorpionOne50", L"UnderwaterSpy77", L"SeagullTamer", L"BitterSecret",
        L"SeeUFriday", L"Gearhead46", L"SergeantSummer", L"SeriousSteve35", L"Sevarog3", L"Seven8Nine93", L"ShadowArrow58", L"Sapphire59", L"SilverBear89", L"BubbleHunter22",
        L"ShieldHorse63", L"Shepard52", L"SwampJackal", L"ShiniestBovine", L"ShogunGiraffe", L"Brainshocked0", L"Shoot2Thrill25", L"Shotdrop87", L"ShootyMcGee40", L"ShoTTy4Life48",
        L"Sharkboy61", L"Shotty2thebody66", L"ShottyBOI1", L"SickPapaya", L"ShrimpHeaven74", L"Shrimpz62", L"SilverySliver", L"SilentBanana17", L"SimplyTheBest97", L"SirChimera83",
        L"SirTricksALot21", L"SizzlinChamp", L"SixesAndSevens65", L"SkeletonMonkey14", L"Slammie9", L"sleepingmantis47", L"SlickElephant", L"SleepyPete56", L"SMGsRdaBest", L"Smellwonk",
        L"Pickles60", L"SlurpItUp93", L"Sn00tyMagician", L"SneakyBush9898", L"SniperCharlie92", L"JakeTheSnake7770", L"SnipersDelight50", L"Reaper1487", L"SmittenKitten4", L"GetScoped92",
        L"SoFastMan", L"Sniper36094", L"Snowday95", L"Solocamper8", L"SoggyCookie26", L"Renegade53", L"SoloSpy86", L"SonicComet37", L"SoulIntellect", L"SoSweaty63",
        L"SoulSpecter", L"SpaceRivers", L"BridgeBuilder87", L"Soundthealarm18", L"SpaceChimp18", L"SpaceLight13", L"SpaghettiCode77", L"SpiderMitten", L"SpokenWard", L"SpookyLeader21",
        L"SpeedyTheCat98", L"SquadMaster22", L"StaleGrapes", L"SporkMaster20", L"StarryGolds", L"SquadUp98", L"Squar3R00t5", L"AtTheBeach321", L"StarWarden80", L"StayHydrated41",
        L"SteelGoose18", L"StoicAlmond", L"TheFreezer21", L"StickyFingers60", L"Sugartech", L"StrangePlant87", L"StringCheese15", L"Sundown57", L"Subversa39", L"GoodIRL",
        L"SuperPoshGrape", L"SuperCandyGurl77", L"Syrupmancer", L"TacoArmada", L"SuperDogCat43", L"SurfRider10146", L"Swabbie23", L"TacticalFish64", L"TacoHands33", L"TAgYOuRIt9",
        L"TaterTots6789", L"Tamerlan30", L"Telepwn78", L"TearCollector18", L"Telltool", L"TankBro85", L"M0arMinutes", L"TepidPorridge", L"FroyoLicker", L"ThatHURTS30",
        L"DangerZero32", L"Thannis80", L"QueenBeet74", L"CatsClause", L"TheBestThereIs45", L"Tacocat60", L"FenderBlender7", L"TheFlopperKing", L"MorninWaffles", L"FishermanJoe86",
        L"MyNameIsHelga15", L"SlurpMaster650", L"OnetrickWonder64", L"Stinger85", L"WindedCrusader", L"Monkeyking17", L"ThreeJabLad", L"NullAndVoid58", L"Notatest22", L"ThermalDragon39",
        L"ThisSideUp87", L"GunShowTickets", L"TigerDreads", L"ThunderKarma33", L"WowerZower", L"TinyMcLarge", L"TickTickBoom57", L"Tigertooth91", L"Time2Go80", L"TLDR4life",
        L"TigerSmash3", L"TinyMoose50", L"DrumGunnar", L"TopDanger28", L"TotallyJeff", L"TreesapBetty", L"BestRegards81", L"Triangulated86", L"TryAgain49", L"FreshFish71",
        L"TurtleUp12", L"TwoHitWonder97", L"TwoWeeks18", L"UndeadBadger", L"UltimateSteve26", L"Valley6789", L"UltraDavid26", L"Untouchable92", L"Mascot2458", L"Vectron29",
        L"PureBacon94", L"VeggieBurger24", L"WaffleDoctor", L"VisionRunner18", L"WanderAgent", L"CyborgWalrus6", L"WalkInThePark66", L"WildfireWizard", L"WalkTheDog80", L"Vertigo70",
        L"Warning1496", L"WildCactusBob", L"Warsiren14", L"Whitman40", L"WeekendWarrior81", L"WickedWasp10", L"Wildonkey", L"Wondertail", L"WrathofKnight", L"Wolfman11",
        L"WolfPhantom46", L"WiggleShark28", L"XenoBotanist27", L"XxBonemealxX", L"xX360NoScopeXx28", L"xXAnonymousXx30", L"Yarley0001", L"HoorayBeets", L"XxCoolNamexX63", L"XxR3AP3RxX64",
        L"YoureNotYour", L"YeetLife35", L"YellowSauce80", L"YourBestFriend63", L"UltimateZenith14", L"Bubbles41", L"Yeetman57", L"Scorpio62", L"brun026", L"leoz1nnn1",
        L"JoyxxxRJ", L"XsniperzX1", L"royaleking15", L"loco^das^lhamas", L"segura_meu_HS", L"M\u00E1nu_M\u00E1zinha", L"capivara16", L"TheM\u00F4nia1", L"cau\u00EA_sp_97", L"juliano\u00CD\u00CD\u00CDBR",
        L"dani_gato1", L"jogadorsemnome05", L"marc\u00E3oBA_BR", L"Jo\u00E3o_SP", L"Extrupicio3", L"semnome0", L"torpekills", L"pAnDoRa10", L"n00bk1ller", L"tligtlig",
        L"lek_rj_br", L"2jo\u00E3ozinho_bh2", L"carniceir00", L"XXplosiv00", L"topzer@@@", L"k1\u00ED\u00ED3R", L"filipinh0sp", L"mr_jonjon", L"zzPAPAICHEGOUzz", L"kt1r0_100",
        L"biel~30", L"chinela_voadora1", L"`tigredoagreste`", L"&darkBeast&", L"XxdivadaskllxX", L"pulgaPG07", L"dead_docks", L"x0baiacubolad0x", L"PatatinoSalvahe", L"DestruyenFritas",
        L"disloka01996", L"xXxdark_tonixXx", L"pedrileanguile", L"AngryMiau", L"locodelcoco8836", L"asdruval_18", L"pepinovski92", L"se\u00F1orsentin", L"ginga871", L"detrasdeti777",
        L"pimpampum2019", L"crazycharly992", L"burlaosquad99", L"Lallamaquellama1", L"illomenuohersot", L"M1st3r0wn3r", L"Rrrrumba98", L"dante_killer12", L"MrPiumPium", L"LordHamburguesa1",
        L"Aplastafritas", L"El_Pr098", L"PatatasHazard", L"ZeroHamburgesa", L"Robak1lls", L"Chimuelopr0", L"Pr0K1ller07", L"NotDefaultPlayer", L"UltimatePr0Play", L"LlamaSpecialist0",
        L"Best_Llama_Ever", L"PatatasScope", L"JustAnotherPro42", L"CrazyAlpaca98", L"gingerbunny125", L"pineappleghost777", L"D34THBR1NG3R12", L"Deadly_Sparkles"
    } },
    { "PhoebeNames_ASIA", {
        L"\uB530\uB974\uB989\uB530\uB974\uB989TRR", L"\uB9C8\uB77C\uD0D5MTGR", L"\uC7A5\uB798\uD76C\uB9DD\uC878\uBD80", L"\uB098\uAC00\uC2E0\uB2E4\uD53C\uD574\uB77C", L"\uCFE8\uCFE8\uC878\uB9AC\uB2E4zz", L"\uC601\uC564\uB9AC\uCE58$", L"\uB9DB\uC788\uB294\uC0E4\uBE0C\uC0E4\uBE0C", L"\uC5B4\uCC28\uD53C\uC6B0\uC2B9\uC740\uB098\uC694", L"\uAC1C\uAD6C\uB9AC\uB4B7\uB2E4\uB9AC98081", L"\uBB34\uAD81\uD654\uC0BC\uCC9C\uB9AC\uC790\uC804\uAC70",
        L"\uC2F1\uAE00\uBC99\uAE00\uD558\uB8E8\uBB3C\uACE0\uAE30\uD30C\uB2E5\uD30C\uB2E5", L"\uC77C\uB354\uD558\uAE30\uC77C\uC740\uC774\uB798\uC694", L"\uC584\uB9AC\uC584\uB9AC\uC584\uB77C\uC16927887", L"\uAF2D\uAF2D\uC228\uC5B4\uB77C31847", L"\uD3FC\uC0DD\uD3FC\uC0AC84059", L"\uBA38\uB9AC\uAC00\uB7752081", L"\uCFE8\uAC00\uC7745959", L"\uC0C8\uD558\uC580\uC6B0\uC720", L"\uC0C8\uAE4C\uB9CC\uCD08\uCF54\uC6B0\uC720", L"\uB208\uB204\uB09C\uB09841318",
        L"\uB6B1\uB6B1\uD55C\uBC14\uB098\uB098\uC6B0\uC720", L"\uD1B5\uD1B5\uD55C\uBA54\uB860\uBE59\uC218", L"\uC7A5\uBC0B\uBE5B\uB538\uAE30\uC6B0\uC720", L"\uBA39\uACE0\uC2F6\uB2E4\uC218\uBC15\uBE59\uC218", L"\uD638\uB864\uB864\uB85C\uB864\uB85C22540", L"\uB9C1\uAC00\uB9C1\uAC00\uB9C154425", L"\uD30C\uC2A4\uD154\uD1A4\uBB34\uC9C0\uAC1C", L"\uBE14\uB9C1\uBE14\uB9C1\uC0E4\uC778", L"\uD3EC\uD150\uD130\uC9C4\uD558\uB8E8", L"\uD06C\uB871\uD06C\uB871krkr",
        L"\uAE61\uCD1D\uAE61\uCD1D\uD1A0\uB07C", L"\uB69C\uAE30\uB69C\uAE30\uBA54\uB69C\uAE30", L"\uC5B4\uB098\uB354\uB808\uBCA8ii", L"\uC544\uBE0C\uB77C\uCE74\uB2E4\uBE0C\uB77C40456", L"\uBD88\uD0C0\uB294\uACC4\uB780\uB9D0\uC7743300", L"\uAE4C\uC545\uAE4C\uC545\uAE4C\uB9C8\uADC0", L"\uC2F1\uC2F1\uD55C\uBA54\uB860", L"\uC9F9\uC9F9\uC774\uCC38\uC0C8", L"\uC704\uB300\uD55C\uC800\uB141\uBC25", L"\uBA4B\uC7C1\uC774\uC2E0\uC0AC\uB098\uAC00\uC2E0\uB2E410694",
        L"\uC601\uD63C\uC758\uBE14\uB799\uD640", L"\uD544\uC2A4\uD398\uC15C\uD22C\uC720", L"\uB10C\uB098\uC758\uB370\uC2A4\uD2F0\uB2C8", L"\uAD6C\uB9AC\uAD6C\uB9AC\uAC1C\uAD6C\uB9AC78511", L"\uC74C\uBA54\uC74C\uBA54\uC1A1\uC544\uC9C028162", L"\uC774\uC81C\uADF8\uB9CCTMI", L"\uD50C\uB809\uC2A4flex", L"\uC2E0\uB09C\uB2E4\uC7AC\uBBF8\uB09C\uB2E4", L"\uC990\uAC70\uC6B4\uB180\uC774", L"\uBAA8\uB450\uD568\uAED8\uC990\uACA8\uC694",
        L"\uD2B9\uBCC4\uD55C\uC120\uBB3C", L"\uD765\uC62C\uB9AC\uACE0\uC62C\uB9AC\uACE0", L"\uBC14\uB78C\uC758\uC18D\uC0AD\uC78433", L"\uBE44\uC624\uB294\uB0A0\uD30C\uC804", L"\uAE68\uB057\uD55C_\uACF5\uAE30\uCCAD\uC815\uAE30", L"\uAE68\uB057\uD558\uAC8C\uB9D1\uAC8C\uC790\uC2E0\uC788\uAC8C", L"\uAFC0\uBC8C\uACFC\uD53C\uC544\uB178", L"\uAC74\uAC15\uD55C\uB2E8\uB9DB", L"\uC9C1\uC811\uC218\uD655\uD55C\uC624\uB514", L"\uC2E0\uC18D\uD55C\uBB34\uBE0C\uBA3C\uD2B8",
        L"\uD06C\uB808\uC6A9\uADF8\uB798\uC6A9", L"\uC218\uC81C\uC624\uB514\uC7BC\uC7BC", L"\uBABB\uB2E4\uC774\uB8EC\uAFC8", L"\uC120\uC120\uD55C\uAC00\uC744\uBC14\uB78C", L"\uC544\uCE74\uC2DC\uC544kkul", L"\uBE45\uD1A0\uB9AC_v", L"\uCE74\uC624\uC2A4\uADF8\uC790\uCCB4", L"\uB2C9\uB124\uC784\uBA38\uD574\uC57C\uD574", L"\uC624\uB298\uC740\uD53C\uC790\uB098\uC787", L"\uB098\uC228\uC740\uACE0\uC218\uC784",
        L"\uC57D\uC624\uB974\uC9C0\uC6A9\uC6A9", L"\uB530\uB73B\uD55C\uC190\uAE3855089", L"\uC800\uB141\uBC25\uBA38\uBA39\uC9C0", L"\uAE30\uAC00\uB9C9\uD78C\uC0F7\uBC1C", L"\uC544\uB2C8\uC774\uB7F4\uC218\uAC00wow", L"\uC544\uBB34\uAC83\uB3C4\uBAB0\uB77C\uC69424728", L"\uC9C0\uAE08\uC774\uB2F7\uB098\uC6B0N", L"\uB3D9E\uC11CW\uB0A8S\uBD81N", L"\uAC1C\uC6B8\uAC00\uC62C\uCC59\uC774", L"\uC6B0\uBB3C\uC548\uC5D0\uAC1C\uC560\uAD6C\uB9AC",
        L"\uAC00\uB0D8\uD508\uB3CC\uACE0\uB798", L"\uAC70\uCE5C\uD30C\uB3C4\uC11C\uD551", L"\uBC30\uACE0\uD508\uAC00\uBB3C\uCE58", L"\uB4F1\uD478\uB978\uACE0\uB4F1\uC5B4BB", L"\uD654\uB824\uD558\uC9C0\uB9CC\uBAA8\uB358\uD558\uAC8C", L"\uC624\uCF00\uAC00\uB294\uAC70\uC57C", L"\uC790\uB2C8_zzZ", L"\uC5B4\uD5C8\uC57C\uB514\uC5B4\uCC28", L"\uBC31\uC218\uB294\uB098\uC758\uAFC8", L"\uCE58\uCE74\uCE58\uCE74\uBC14\uC0AD\uBC14\uC0AD",
        L"\uB208\uCE58\uCC59\uACA8\uB300\uD654\uCC28\uB2E8", L"\uC815\uD655\uB3C4\uAC00\uC0DD\uBA85", L"\uAC74\uC124\uC5B4\uB835\uB2E4\uD4E8\uD4E8", L"\uB0B8\uB4E4\uC544\uB2C890", L"\uC1FC\uD0C0\uC78433274", L"\uD06C\uB808\uC774\uC9C0\uAE09XD", L"\uD558\uB8E8\uC0B4\uC774\uC778\uC0DD09", L"\uCDA4\uCD94\uB294\uACB8\uB465\uC774", L"\uB2E4\uAC19\uC774\uB178\uC62C\uC790\uC544", L"\uC790\uC544\uC2E4\uD604\uB05D\uD310\uC655",
        L"\uC644\uBCBD\uD55C\uBA40\u314CI", L"\uC22D\uAD6C\uB9AC\uB2F9\uB2F9\uC22D\uB2F9\uB2F911211", L"\uAC01\uB09C\uB2EB\uB784\uB9D8\uBC25\uC0BF31445", L"\uB3C4\uB780\uB3C4\uB780\uC54C\uD1A0\uB780", L"\uB098\uC058zl\uC54A\uC740\uB290\uB08C", L"RE\uC988\uC6F0", L"\uB8E8\uC2DC\uB4DCPL", L"\uC544\uAE30\uC2F1\uC5B4\uC1A1ss", L"\uB9C8\uB978\uD558\uB298\uB0A0\uBCBC\uB77Di", L"\uD0A4\uC7207",
        L"\uC720\uB808\uCE742009", L"\uBE14\uB77C\uBE14\uB77Caa", L"\uCD95\uBC30\uB97C\uB4E4\uC5B4\uB77C\uC9E0", L"\uBBF8\uC2A4\uD130\uC9C0\uB2C8", L"7\u314F\uC2DC\uB098\uBB34", L"\uBC30\uCE58\uACE0\uC0AC\uC911", L"\uC2DC\uBC94\uC6B4\uD589\uC911\uC784\uB2E4", L"\uB0B4\uAC00\uCE90\uB9AC\uD55C\uB2E41", L"\uB2C8\uCF54\uB0B4\uCF54\uC7E4\uCF54", L"\uCE89\uCE89\uB304\uC218ss",
        L"\uC778\uC131\uACFC\uC2E4\uB825\uC744\uACB8\uBE44", L"\uB10C\uC774\uC81C\uC544\uC6C3t", L"\uCC29\uD55C\uCC38\uCE58chi", L"\uD2F0\uB9E5\uC2A4\uBD09vv", L"\uB2E4\uC2DC\uB3CC\uC544\uC628\uBD80\uBA54\uB791", L"\uD2F0\uB04C\uD0DC\uB04C\uB9E4\uB04C", L"\uAEC4\uAEC4\uB300\uB9C8\uC655XD", L"\uB098\uB294\uC57C1999", L"\uC544\uB9AC\uC628333", L"\uD0C8\uC8FC\uBC29\uC9C0\uC704\uC6D0\uD68C",
        L"\uD63C\uC7901\uC654\uC5B4\uC694", L"\uAC19\uC7742\uC654\uC5B4\uC694", L"dustn\uB354\uC2A4\uD2BC", L"\uB9DD\uD0A4s\uC2A4\uB098s", L"\uAF3C\uB0E5\uAF3C\uB0E5\uB0E5\uB0E5", L"\uB098\uC774\uC2A4\uD55C\uD558\uB8E834", L"\uC9DD\uC9DD\uC9DD\uBC15\uC218\uC9DD\uC9DD\uC9DD", L"\uD2B8\uC719\uD074\uBC84\uB4DC09", L"VV\uADC0\uC5FD\uAE5C\uCC0DVV", L"\uD558\uB9C8\uCC38\uCE58\uD558\uB9C8\uCC38\uCE58",
        L"\uC0C1\uD07C\uD55C\uB0B4\uC77C\uD558\uD2B8", L"\uC5F0\uC8FC\uBB34\uD7345\uB9AC\uBB34\uC911", L"\uBBF8\uAE08\uC5ED7\uBC88\uCD9C\uAD6C", L"\uBD80\uB18D\uAC85\uB4C0s2", L"\uC9C0\uD558\uCCA0\uC544\uB2EC\uB824\uB77C", L"\uCF54\uC624\uBFD4\uC18C\uC6241999", L"\uD559\uACA8\uC2DC\uB7EC\uC6A9\uC6A9", L"\uBE44\uC0C1\uBE44\uC0C1\uBE44\uC0C1", L"\uD06C\uB808\uD30C\uC2A4\uC8E0\uC624", L"\uBCA0\uB85C\uB2C8\uCE74\uB808\uBAAC",
        L"33\uB2EC\uCF64\uB7EC\uC26C33", L"\uBAA8\uC758\uACE0\uC0AC\uB0E0\uB0E0", L"\uC990\uAC81\uACE0\uC4BC\uB098\uC694", L"10041400\uC6A9\uC6A9", L"\uCF69\uCF69\uC2A4\uCE74\uC774\uCF69\uCF69", L"\uD38C\uD504\uC0B4\uB824\uB0B4\uC5D0", L"\u3086\u304D\u3086\u304D1999", L"9\u67089\u65E5\u4ECA\u65E5\u306F\u6674\u308C", L"\u304A\u3053\u305F\u306E\u5B63\u7BC0", L"200\u5186\u6255\u3048\u3070\u7121\u6599",
        L"\u305B\u305B\u3089\u304E2029", L"\u305D\u306E\u9B5A\u3092\u4FDD\u8B77\u3057\u3066\u304F\u3060\u3055\u3044\u3002", L"\u3060\u3044\u304D\u30D6\u30FC\u30D6\u30FC", L"\u30B5\u30DC\u30B5\u30DC\u30B5\u30DC\u30C6\u30FC\u30F3", L"\u307F\u304B\u3093\u306E\u751F\u7523\u5730", L"\u30E9\u30C3\u30B3\u3068\u3084\u3082\u308A", L"\u304B\u305A\u305F\u306E\u30DE\u30DE", L"\u3059\u305A\u3081\u30C1\u30E3\u30F3", L"\u597D\u7269\u306F\u5375\u304B\u3051\u3054\u98EF", L"\u51AC\u304C\u82E6\u624B\u30CB\u30AD",
        L"\u30BF\u30D4\u30AA\u30AB\u5973\u5B50\u9AD8\u751F", L"\u305F\u3053\u3084\u304D\u7537\u5B50\u9AD8\u751F", L"\u306F\u307F\u308B\u3068\u3093\u30A6\u30A7\u30FC\u30A4", L"\u3084\u304D\u3044\u3082\u3055\u3068\u3044\u3082\u3055\u3064\u307E\u3044\u3082", L"\u305F\u3044\u3084\u304D\u3069\u3089\u3084\u304D", L"\u968E\u6BB5\u306E\u5302\u3044\u3092\u304B\u3044\u3060\u3093\u3060", L"\u671D\u306F\u3054\u306F\u3093\u6D3E", L"\u3046\u307F\u306E\u30532000", L"\u3053\u308C\u98DF\u3079\u305F\u306E\u3060\u308C", L"\u30AA\u30BF\u30AF\u5927\u597D\u304D",
        L"\u3053\u3053\u3067\u4E00\u53E5", L"\u52DD\u8CA0\u3042\u308A", L"\u5668\u7528\u8CA7\u4E4F98081", L"\u671D\u306F\u30D1\u30F3\u6D3E27887", L"\u304A\u3042\u3068\u304C\u3088\u308D\u3057\u3044", L"\u4E94\u672C\u6307\u30BD\u30C3\u30AF\u30B931847", L"\u30BF\u30D4\u30AA\u30AB\u30D4\u30FC0", L"Harupinpon2015", L"Mapo0510", L"\u30CF\u30ED\u30FC2002",
        L"\u306A\u3093\u304F\u308B\u306A\u3044\u30B5", L"\u3042\u3084\u592A\u90CE02", L"\u304B\u305A\u885B\u95801995", L"\u82BD\u8863Queen", L"\u6E2F\u306E\u3086\u3046\u3053", L"\u304F\u308D\u3055\u3093\u3061\u3073\u3055\u3093", L"\u30B8\u30E7\u30F3\u541B\u53C2\u4E0A", L"\u98DF\u3044\u3057\u3093\u574A\u3072\u307E\u3061\u3083\u3093", L"aiko1.9.1998", L"\u30DD\u30C6\u30C8\u30C9\u30EC\u30C3\u30B7\u30F3\u30B0",
        L"\u30AA\u30E0\u30E9\u30A4\u30B9\u65E5\u548C", L"\u6728\u66DC\u65E5\u306F\u7D66\u6599\u65E5", L"\u65B0\u5175\u304F\u308D\u306D\u3053", L"\u6CE5\u68D2\u3092\u8A31\u3059\u306A", L"\u307E\u308B2017", L"\u307D\u3093\u54092\u53F7", L"\u306F\u3063\u3071\u3067\u307E\u3063\u3061\u3083", L"\u62B9\u8336\u5473\u30AA\u30EC", L"\u4E95\u304B\u3089\u51FA\u305F\u86D9\u5927\u6D77\u3092\u77E5\u308B", L"\u9B5A\u306E\u8033\u306B\u5FF5\u4ECF",
        L"\u3089\u3073\u3063\u3068\u968A\u9577", L"\u304D\u3044\u305F\u3093\u307D005", L"\u67FF\u304B\u3089\u751F\u307E\u308C\u305F\u67FF\u592A\u90CE", L"\u307F\u3046\u3089\u304A\u306B\u3044\u3055\u3093", L"\u3075\u308F\u3075\u308F\u3061\u3083\u3093\u306D\u308B", L"\u3082\u3063\u3061\u30FCPM3:00", L"\u5E38\u590F\u30B3\u30B3\u30CA\u30C3\u30C4", L"\u53F0\u30D1\u30F3\u3056\u3080\u3089\u3044", L"\u4E00\u5BF8\u5E3D\u5B50", L"\u308C\u304A\u304F\u30931267",
        L"\u306B\u3093\u306B\u304F\u5FCD\u8005", L"\u3082\u3053\u3082\u3053\u30DE\u30EB\u30B2\u30EA\u30FC\u30BF", L"\u5973\u5929\u4F7F\u3068\u3082\u308A\u3093", L"\u3059\u308A\u6797\u6A8E\u30DC\u30FC\u30A4\u30BA", L"\u304F\u308B\u304F\u308B\u30C1\u30E7\u30B3\u30EC\u30FC\u30C8", L"\u307F\u304B\u3093\u306E\u795E\u69D8"
    } },
    { "PhoebeNames_ME", {
        L"0PingSting", L"12Siblings12", L"2xPump4Life", L"_BigBainPlayz_", L"-=Fr0stDevil=-", L"1eyedInvent0r", L"360walkaway", L"42Wow47", L"2sweaty23", L"0mn1v0r378",
        L"PanicButton47", L"1Forest120", L"8BallWasHere", L"4Regionals", L"90sRunner", L"A Sweaty Dog", L"SpectrumKnight13", L"aaaaaaaaalbert", L"4tnite4ever15", L"aaaaaleeex",
        L"ADudeNamedPhil", L"90cranker7", L"AGenuineHuman", L"ActivateFail", L"aClassyGamer94", L"Agoraphone-8", L"AGoodOmen", L"Agua_Cat", L"Ahat4aMouse", L"AggroHamster59",
        L"Aim Like Idaho", L"Albacore6", L"airplanelegs", L"AligatorPear", L"TheBe$t", L"Allsmyles:D", L"AllTheCandy", L"Archibard11", L"amygdallama", L"AmpleBlindspot",
        L"ALLCAPS67", L"AlphaPlayer34517", L"ArcticAndroid", L"AlwaysOnTime38", L"Abaddon74", L"Angrification", L"Anonymutt", L"Alternative74", L"APeelySmoothie", L"Ambrosia0",
        L"AngryRabbit9", L"HippoMagician", L"AngryRoommate10", L"SweetPenguin16", L"Arclite28", L"CarpMedley", L"SaberPhoenix75", L"AthenaOrApollo", L"AtomicTerr\u221A\u2264r", L"AttackOLantern",
        L"AngryDuck51", L"Atomizer5", L"AyeBall23", L"AngelEye30", L"Primefall44", L"Armstrong78", L"Baethal", L"BabySeahorse81", L"StoneDragon61", L"Back2Work61",
        L"DarkTarget12", L"Bag0Hammers", L"GerbilTrapeze", L"BadKahuna94", L"BananaPastrami", L"Banandana82", L"BattleHammer7", L"ToxicTropic", L"BagelBoy82", L"Balefire81",
        L"JonesyWannabe46", L"Barktooth81", L"Beebitme", L"SlothLife16", L"BeepGoesBeep", L"Represent82", L"BearPuns55", L"Belltop", L"BertIsNice", L"BeeHive7",
        L"BetaChampion", L"BehindYou60", L"BellyFlop40", L"BioDragoon", L"BeefSupreme35", L"BitOJoy", L"BillyBrilliant23", L"BigBuddy15", L"BlinkImGone44", L"Blizzardbeard77",
        L"BlipBlap53", L"BlindCat47", L"Blueberry1002", L"BoatRider10", L"Boomstick59", L"BoatingIsLife", L"Bobtimus", L"BoldPrediction", L"Blackjack31", L"BonyHumans",
        L"Bongodrum010", L"BobsUrUncle71", L"BootifulSnow", L"BotheredBud1", L"BootThisUp256", L"BoxOfGoblins", L"BorisTheGreat37", L"BrainInvader", L"BroSincerely", L"BraddleRoyale80",
        L"Bro2Max", L"BrandNew47", L"BreadSandwich42", L"BeRooted", L"Brotato3131313", L"Brometheus52", L"BossBoi20", L"BreakerBreaker58", L"Bronson66", L"BrokenRecord26",
        L"CryptoDancer", L"BubbleDragons", L"BubblyFalcon", L"Nocturne9964", L"BrutalNoodle91", L"BatTacos", L"Brutalis42", L"Buffering5", L"BumbleB3ar", L"Bundledbats",
        L"BurgerGuy1011", L"PuppyDance9", L"BusinessJoe", L"ButchSquire", L"BuddyGuy9518", L"BushCamper2477", L"butfirstCOFFEE67", L"ButternutQuash", L"BewareTheGOAT", L"ButterKid24",
        L"C-Block54", L"CactusDad80", L"SublimeCactus24", L"CafeDave58", L"CandyPlumber", L"CovertClown95", L"CannotDo42", L"CaptainKiernan", L"CantTouchMe67", L"CantHearYou3",
        L"CarneAsideOf", L"Skyfire85", L"Carcrusher94", L"Caspian79", L"Cathode57", L"Centice49", L"BlackCat61", L"CherubTree", L"Channel2659", L"Ch0pp3r66",
        L"CharmedImSure85", L"ChestOfFault", L"Cannonball86", L"Primetime71", L"Cheese10128", L"iHazHighGround", L"ChickenFarmer17", L"Lexington90", L"ChiliDog87", L"ChillMono",
        L"Chorizo1337", L"Chronicled5", L"Chug_Brigade", L"ChArter52", L"Chronolink24", L"FootballDay", L"ColonelChugJug", L"CleverPanda91", L"Chipper70", L"CleverPun92",
        L"Cleopatra30", L"CoffeeFueled84", L"ConstantThorn", L"CounterMantra", L"ColonelFox41", L"ComingForYou19", L"CopperBottom43", L"Corrosion86", L"CrabInMyShoe", L"CowJuice92",
        L"CrepeSalad", L"CrazyJet83", L"CopyKat58", L"CrazyPea96", L"CrocodileBard", L"CruiseOn", L"CrabbyBaby71", L"Crestfallen64", L"Crypt1c60", L"CtrlAltDeleted94",
        L"CyborgMelon", L"CrumbBum91", L"Curveball20", L"d0m3dw3ll3r", L"N0nDa1ry", L"CyberWolf26", L"DanglingKeys", L"DarlingSnail", L"D3m0nk3y56", L"Dawson201",
        L"Damarus83", L"DanIsHere50", L"DeepStealth", L"DefiantGerbil", L"DisIsMatt", L"DekesNdangles", L"2Xtrouble13", L"DemonOnWheels", L"BraunyBanana", L"Dinosaur4Hire",
        L"DJHoneybun", L"Dessloch52", L"Dimitra66", L"DiamondQuartz78", L"Disconnect21", L"Divisor78", L"Caveman59", L"DoctorGungHo", L"DrQuikMaths", L"PopNLock7",
        L"Doilies4Days", L"DoctorLobby92", L"Strider0728", L"DoublePinpoint", L"BestDoggo83", L"DoubleDaring", L"DoubleBlitz52", L"DoubleDuel75", L"DoublePumpkin78", L"DoubleRainbow96",
        L"DrPlanet", L"DoUeven84", L"Dreamdunker", L"DramaAlert5", L"Doyourdishes33", L"Drekorig97", L"DJLightning85", L"Drimacus7", L"DrPlants40", L"Dunkball96",
        L"DeluxeDino18", L"DreamDog18", L"EarOfCorn1010", L"Eggwithnosalt", L"EdgeGoose", L"HyenaBrigade", L"GoldenOctopus0", L"LazyLlama7", L"KeyboardKing32", L"JimmyIsOnFire97",
        L"SadTroll94", L"Elftastic", L"Erraatic27", L"Eubanker555", L"ElderFly13", L"Exhale34", L"EndlessMike52", L"F1veFruits", L"EvilJonesy12", L"Fall3nSlasher",
        L"ElPollo85", L"FacelessPanda56", L"FauxFeathers", L"FallingInStyle86", L"FancyFingers87", L"FantasyCruz98", L"PositiveFeels", L"Faraleth90", L"Fictionpoint", L"FavoriteFlavor45",
        L"FightinFishBoy", L"FinlessShark", L"FeasibleWeasel80", L"TimeSplat", L"FishingEvryDay", L"FireCheeks56", L"FireDynasty68", L"5HoleSniper", L"FlamingoGuru", L"FishWithLungs27",
        L"FlankTheTank", L"FlavorCapitan", L"SomethingFishy3", L"FlawlessGame", L"FluffySardine", L"Flyingfender", L"FootBarn", L"Firewall4", L"FlossPatrol82", L"Forge62018",
        L"ForksNSporks", L"ForlornMorning", L"FluffyMuffin8", L"ForsakenVoyage", L"Forbidden60", L"ForkAndKnife17", L"ForkSpoonKnife42", L"Fortifiable", L"FreeCookies", L"FortGoodNite43",
        L"FortniteKid34558", L"Frediculous20", L"FreeMinded49", L"Freylis21", L"FriendlySniper27", L"FrogSpawn42", L"FunkLogic17", L"FunkyFresh1349", L"Frostbitten86", L"FuzzyPickl3s",
        L"g0tmy3Y3onU", L"FuriousBug50", L"GalaxyBrain77", L"GamerGreat30", L"GameWinnerz87", L"Gaargod55", L"Glamour67", L"FunSized86", L"Freefire63", L"GeekTragedy",
        L"GeoForts", L"GiantDaisy", L"Goosezilla13", L"Gatordile81", L"GiaPuppy", L"GhostColour6", L"GigaWatt10k", L"GimmeCrisps", L"GimmeMoar", L"FancyPumpkin87",
        L"GlassCannister", L"GlitterMantis", L"GhostChicken12", L"Giuseppina71", L"Gkublok13", L"GloatingEmu", L"Glopazoid", L"GlowBeetle41", L"GnomeOverlord", L"FlimsyGoat",
        L"GoblinSelfie", L"GnomeRider16", L"GoldenCardamom", L"Goldie971", L"GoldenMage12", L"GoldenManatee6", L"GolfVoid", L"Going4Gold17", L"GoneFishin15", L"GardenDolphin",
        L"GradedOnACurve", L"GotHeartburn", L"gonnagetcha15", L"Gooddoggo80", L"Gigabyte51", L"GreasyGoal82", L"Greedy33", L"Initialize0", L"Dominator25", L"Gridl0cked",
        L"Grandma40", L"GrannyGunner3", L"GreatGriffin28", L"GoodDawg13", L"GrumbleofPugs6", L"GrimRanger72", L"Guacamoldy7", L"GuardianHeart4", L"GuessWhat23", L"Gurshwunner",
        L"H1pC0c0nut", L"HaaalpMe", L"GunGoesPewPew38", L"HangryBuddy", L"HamsterTaxi", L"Hydrohawk7", L"HaHaHeeHee73", L"HalfEatenPizza82", L"HandsomeLemon34", L"HashTagonistic",
        L"HauntedDreams", L"HappyMelon50", L"HazardousPaste", L"HashtagToad57", L"HeliumHog", L"Here4Dancing", L"HawkWrath74", L"HermitCrab26", L"Hevymeddle", L"PlzBeNiceToMe",
        L"HopOnDown20", L"HelloMyDude54", L"Hashbrown31", L"HollowBrick5", L"HeyThereFriend81", L"MajesticMutant", L"HeroicHealer71", L"Hoodwinked12", L"Hiveminder97", L"HotelBlankets",
        L"HowAreMy90s", L"Horse4227", L"HubcapDonuts", L"SpringChicken10", L"ISeeCiabatta", L"Howitzer98", L"iAteMySpacebar", L"IAmW4t3r", L"IronHype58", L"HomeSkillet95",
        L"Hollywood70", L"IckyWombat", L"ifyougotem", L"IGottaGoToBed", L"IceFork43", L"Ichthys25", L"IHaveFullBrick", L"IGotSticks85", L"IhaveAGoodElo", L"IHaveFullMetal",
        L"IHateBacon35", L"ImGonnaWin2234", L"InflatedCouch", L"InsideAgent", L"LetsBePals23", L"ImFam0us24", L"InfinityPigeon66", L"ImReallyADog76", L"IntoTheOcean", L"InverseUnicorn95",
        L"IronBoss86", L"IrritatedIris", L"IsBruceHome", L"InnoScent24", L"IronMade20", L"Irontide57", L"SalsaVerde50", L"SilentRogue98", L"IvanDude0001", L"IsOnlyGame12",
        L"IamPizza5", L"BillysMom17", L"ElectricKitten12", L"MarathonWalker", L"LostYeti57", L"MagicJackie8", L"JebidiahSparks", L"VeryMuchGG", L"TallestAnt4", L"JewelOfWisdom",
        L"JellyBeats81", L"JollyGoat46", L"JustSugar85", L"JonnyTheKid52", L"JonesyForever35", L"ImmortalMango66", L"Happy2Bhere39", L"JumpyMcJump64", L"JackalViper", L"JustBuster73",
        L"TermiteMuscles", L"KenCrunch", L"KeyFlamingo", L"VerticalSlice", L"Juxtapose48", L"KatKat200239", L"WigglyCow", L"KingFlexor", L"SimplyKraken", L"KeepScrolling95",
        L"KingOfFudge", L"ThroneBlade47", L"KimchiFighter13", L"BoogieKing85", L"Knowaguy", L"KingToucan2", L"KingKoala82", L"FickleKnight20", L"DoomCrow74", L"ColorfulCoyote",
        L"Kragoth93", L"Quack4Bread", L"Kregore73", L"Shadow74739", L"LambFury", L"LavaHound68", L"JungleProwler25", L"1337Sk1lz", L"Landsnail80", L"LarsTheKid25",
        L"Laserfrog44", L"ManiacMeerkat49", L"JustABitEpic", L"LewtGoblin7", L"Liftoff36", L"Lightsp33d79", L"LilBandit94", L"LilGrande56", L"LittlePidgeon", L"LaidBackBeats",
        L"LousyCentaur", L"LovelyLemur", L"LobbyBound43", L"LilMeowMeow50", L"DangerLeopard", L"LootTrooper51", L"Mayonnaise48", L"LadyLuck75", L"Lulabyebye", L"LuckyNumber4279",
        L"LuckyLadybug65", L"MagicQuail", L"MaximumMad53", L"Lizardfish49", L"MajorProdigy51", L"majorOOF49", L"MadBrad50", L"Makreth4", L"Mandible13", L"Manwell65",
        L"MarineDog54", L"Malakai96", L"MaximumBro3", L"MaximusIon59", L"MaybeNot40", L"MeanSardine", L"KittyCat80", L"McCucumber71", L"McLawnmower91", L"MegaYeet98",
        L"MemoryGobbler", L"Megacrash21", L"Meshuncle", L"MeLlamoJefe76", L"Meshdaddy85", L"MightyMonkfish", L"MightySobek", L"MidnightScar26", L"Memphis31", L"Marionette84",
        L"Thx4DaLoot14", L"MeowMeow38", L"MissedJumpshot", L"Mission1536", L"MintElephant26", L"RareOrange", L"MisterTaboo", L"MisterMosquito0", L"MongLason", L"MooseRampage",
        L"UltraMom83", L"MondayMourning82", L"MoreOfTheSame", L"MoonWizard76", L"MsFroggy123", L"Monolith95", L"Mouthful95", L"SpeedSparrow47", L"MrBurgers39", L"MayorMVP",
        L"MuddyBoots7", L"Nanochunks", L"JokeFountain", L"SwampRhino", L"MutantGecko7", L"NavyQuail", L"Nashtyboy67", L"NeoDynamic", L"Fearmaster62", L"Mountain83",
        L"NeonSalmon", L"NegativeNight7", L"NocturnePatrol", L"NomnomCarver", L"MysteryMan79", L"NeoHero4", L"NonCarbon8", L"ZeroOverDrive", L"ElegantBeast39", L"TooManyBeets",
        L"NotMyLasagna18", L"Numbers789", L"Nickolai62", L"NoodleSoup83", L"Number141", L"OblongWidget", L"NumeroDos28", L"OddComplex85", L"OldWaterBottle28", L"OleReliable78",
        L"OneMillion17", L"OneGlimpse87", L"OnlyMust4rd", L"OneMillionAnts24", L"OpenShutCase", L"OpenSeason23", L"OrangeTango33", L"Overtime46", L"TroubledTaco45", L"9000Hippos",
        L"Ouboudah25", L"OxfordCommas", L"ItsAlwaysMe", L"Othello54", L"Ophelia66", L"Paracord", L"PopcornSalt3", L"PapaParadise82", L"ParanoidCactus", L"PebbleSpider",
        L"NotAPalindrome38", L"PapaJimbo9", L"PepperTaxi", L"PeskyInfant", L"PickaxeUAQ", L"FrogTheCat18", L"Smoothie66", L"PizzaUnic0rn", L"PerpleXed79", L"Pinnacle6",
        L"PlatinumPony", L"PirateWizard22", L"PlayOnRepeat", L"Playmaker365", L"PlasticBullets62", L"PoachedPepper", L"PortableOx", L"PortaSniper", L"PLZST0P75", L"pleaseletmewin10",
        L"PotatoCupid", L"ViciousPie44", L"PowerFlank", L"PrancingPwnee", L"PrimeSystem", L"PrinceWombat", L"PrincessImp", L"PotsNPans39", L"Prisoner19A", L"PsyonicOx",
        L"PurpleRocks99", L"QuartetUltra", L"QuantumFig", L"PumpkinKnight83", L"PureSneak92", L"QuadAlex13", L"QWERTYb0ss", L"R00kMine6", L"RabbitKicks", L"PurpleCrayon85",
        L"R0ckStar5", L"Ramsbottle", L"RainbowZebra36", L"RandoDog73", L"Rainmaker67", L"Ramp4ge71", L"RavensRevenge13", L"RatherBFishin0", L"RealMisfortune", L"RelaxOkay",
        L"Reach4TheSky54", L"RedOrange2240", L"Rhett5408", L"RiskyDigits", L"RagePower81", L"Riftmage54", L"Riboflavin52", L"RoarTiger24", L"GoodRobin6", L"RoyaleCorral",
        L"RocketBoi87", L"Rocking7614", L"RudeAwaken1ngz", L"Roundhouse47", L"SalemSquid", L"RustyShine93", L"SLUGshot65", L"Sabretooth67", L"Salamander8910", L"Sandra6000",
        L"SandSpider2000", L"SalsaPants36", L"SaltySandwich81", L"SappyPirate", L"Sandwitch90", L"SassyJazzhands", L"ScorpionOne50", L"UnderwaterSpy77", L"SeagullTamer", L"BitterSecret",
        L"SeeUFriday", L"Gearhead46", L"SergeantSummer", L"SeriousSteve35", L"Sevarog3", L"Seven8Nine93", L"ShadowArrow58", L"Sapphire59", L"SilverBear89", L"BubbleHunter22",
        L"ShieldHorse63", L"Shepard52", L"SwampJackal", L"ShiniestBovine", L"ShogunGiraffe", L"Brainshocked0", L"Shoot2Thrill25", L"Shotdrop87", L"ShootyMcGee40", L"ShoTTy4Life48",
        L"Sharkboy61", L"Shotty2thebody66", L"ShottyBOI1", L"SickPapaya", L"ShrimpHeaven74", L"Shrimpz62", L"SilverySliver", L"SilentBanana17", L"SimplyTheBest97", L"SirChimera83",
        L"SirTricksALot21", L"SizzlinChamp", L"SixesAndSevens65", L"SkeletonMonkey14", L"Slammie9", L"sleepingmantis47", L"SlickElephant", L"SleepyPete56", L"SMGsRdaBest", L"Smellwonk",
        L"Pickles60", L"SlurpItUp93", L"Sn00tyMagician", L"SneakyBush9898", L"SniperCharlie92", L"JakeTheSnake7770", L"SnipersDelight50", L"Reaper1487", L"SmittenKitten4", L"GetScoped92",
        L"SoFastMan", L"Sniper36094", L"Snowday95", L"Solocamper8", L"SoggyCookie26", L"Renegade53", L"SoloSpy86", L"SonicComet37", L"SoulIntellect", L"SoSweaty63",
        L"SoulSpecter", L"SpaceRivers", L"BridgeBuilder87", L"Soundthealarm18", L"SpaceChimp18", L"SpaceLight13", L"SpaghettiCode77", L"SpiderMitten", L"SpokenWard", L"SpookyLeader21",
        L"SpeedyTheCat98", L"SquadMaster22", L"StaleGrapes", L"SporkMaster20", L"StarryGolds", L"SquadUp98", L"Squar3R00t5", L"AtTheBeach321", L"StarWarden80", L"StayHydrated41",
        L"SteelGoose18", L"StoicAlmond", L"TheFreezer21", L"StickyFingers60", L"Sugartech", L"StrangePlant87", L"StringCheese15", L"Sundown57", L"Subversa39", L"GoodIRL",
        L"SuperPoshGrape", L"SuperCandyGurl77", L"Syrupmancer", L"TacoArmada", L"SuperDogCat43", L"SurfRider10146", L"Swabbie23", L"TacticalFish64", L"TacoHands33", L"TAgYOuRIt9",
        L"TaterTots6789", L"Tamerlan30", L"Telepwn78", L"TearCollector18", L"Telltool", L"TankBro85", L"M0arMinutes", L"TepidPorridge", L"FroyoLicker", L"ThatHURTS30",
        L"DangerZero32", L"Thannis80", L"QueenBeet74", L"CatsClause", L"TheBestThereIs45", L"Tacocat60", L"FenderBlender7", L"TheFlopperKing", L"MorninWaffles", L"FishermanJoe86",
        L"MyNameIsHelga15", L"SlurpMaster650", L"OnetrickWonder64", L"Stinger85", L"WindedCrusader", L"Monkeyking17", L"ThreeJabLad", L"NullAndVoid58", L"Notatest22", L"ThermalDragon39",
        L"ThisSideUp87", L"GunShowTickets", L"TigerDreads", L"ThunderKarma33", L"WowerZower", L"TinyMcLarge", L"TickTickBoom57", L"Tigertooth91", L"Time2Go80", L"TLDR4life",
        L"TigerSmash3", L"TinyMoose50", L"DrumGunnar", L"TopDanger28", L"TotallyJeff", L"TreesapBetty", L"BestRegards81", L"Triangulated86", L"TryAgain49", L"FreshFish71",
        L"TurtleUp12", L"TwoHitWonder97", L"TwoWeeks18", L"UndeadBadger", L"UltimateSteve26", L"Valley6789", L"UltraDavid26", L"Untouchable92", L"Mascot2458", L"Vectron29",
        L"PureBacon94", L"VeggieBurger24", L"WaffleDoctor", L"VisionRunner18", L"WanderAgent", L"CyborgWalrus6", L"WalkInThePark66", L"WildfireWizard", L"WalkTheDog80", L"Vertigo70",
        L"Warning1496", L"WildCactusBob", L"Warsiren14", L"Whitman40", L"WeekendWarrior81", L"WickedWasp10", L"Wildonkey", L"Wondertail", L"WrathofKnight", L"Wolfman11",
        L"WolfPhantom46", L"WiggleShark28", L"XenoBotanist27", L"XxBonemealxX", L"xX360NoScopeXx28", L"xXAnonymousXx30", L"Yarley0001", L"HoorayBeets", L"XxCoolNamexX63", L"XxR3AP3RxX64",
        L"YoureNotYour", L"YeetLife35", L"YellowSauce80", L"YourBestFriend63", L"UltimateZenith14", L"Bubbles41", L"Yeetman57", L"Scorpio62", L"\u0635\u0627\u0645\u062F1", L"\u062C\u0627\u06431",
        L"\u062D\u0645\u064A\u062F21", L"\u0622\u0646\u062B\u0648\u0646\u064A", L"\u0627\u0644\u062C\u0648\u0643\u063185", L"\u0645\u062C\u062A\u0647\u062F1", L"\u062C\u0648\u06461", L"\u0631\u0648\u06431", L"\u0644\u0648\u0646\u063A\u0645\u0627\u0646", L"\u0627\u0644\u0639\u0627\u0634\u06422", L"\u0648\u0644\u064A\u0627\u06451", L"\u062A\u0648\u064A\u0633\u062A1",
        L"\u0631\u064A\u0627\u064685", L"\u0643\u0648\u0646\u0648\u06311", L"\u0643\u0648\u0646\u0631\u0627\u062F", L"\u0641\u0627\u0646\u062A\u0648\u06452", L"\u0645\u0627\u0631\u0641\u06441", L"\u062E\u0627\u0641\u064A\u0631", L"\u0645\u0645\u0644\u0648\u06431", L"\u0643\u0627\u064A1", L"\u0643\u0648\u06441", L"\u0622\u0634\u062A\u0648\u0646",
        L"\u0628\u0627\u063A\u0648\u064A\u0644", L"\u0633\u062A\u064A\u0641\u0627\u0646\u06481", L"\u0646\u0648\u064685", L"\u0623\u0643\u0645\u0644", L"\u0633\u0627\u0644\u064524", L"\u0646\u0648\u0644\u0627\u0646", L"\u0643\u0627\u0645\u064A\u0631\u0648\u06461", L"\u0622\u062F\u06451", L"\u062C\u064A\u0645\u06331", L"\u0633\u0639\u0627\u062F\u06291",
        L"\u0631\u0627\u064A\u062F\u06311", L"\u0641\u0627\u0631\u0633124", L"\u0641\u0647\u06311", L"\u0644\u064A\u062B55", L"\u0645\u0647\u0646\u062F124", L"\u0625\u062F1", L"\u0645\u0643\u0645\u0627\u0647\u0648\u0646", L"\u0623\u062B\u064A\u06311", L"\u0641\u0648\u0631\u062A\u0646\u0627\u064A\u062A\u0627\u0648\u064A \u0639\u0644\u0649 \u0637\u0648\u0644", L"\u0623\u0645\u064A\u0631\u0629 \u0627\u0644\u062E\u0644\u064A\u062C",
        L"\u0633\u0639\u064A\u062F\u0648 \u0627\u0644\u0631\u0647\u064A\u0628", L"\u062E\u0648\u0627\u0631\u0632\u0645\u064A \u0627\u0644\u0623\u0644\u0639\u0627\u0628", L"\u0639\u0627\u0635\u0641\u0629 \u0627\u0644\u0635\u062D\u0631\u0627\u062198081", L"\u0645\u062D\u0627\u0631\u0628 \u0627\u0644\u0635\u062D\u0631\u0627\u062184059", L"\u0627\u0644\u0641\u0631\u0639\u0648\u0646 \u0627\u0644\u0639\u0627\u0634\u064231847", L"\u0646\u062C\u0645 \u0627\u0644\u0646\u062C\u0648\u064541318", L"\u062D\u0645\u0627\u062F\u0647 \u064A\u0644\u0639\u06282081"
    } }
};

static void SetCurve(FScalableFloat& ScalableFloat, UCurveTable* CurveTable, const wchar_t* RowName)
{
    ScalableFloat.Value = 1.f;
    *(int32*)(uint64(&ScalableFloat) + 0x4) = -1;
    ScalableFloat.Curve.CurveTable = CurveTable;
    ScalableFloat.Curve.RowName = FName(RowName);
    *(void**)(uint64(&ScalableFloat) + 0x20) = nullptr;
}

static void SetSoftObject(FSoftObjectPtr& SoftObject, const std::wstring& Path)
{
    auto Separator = Path.find(L'.');

    SoftObject.ObjectID.AssetPath.PackageName = FName(Path.substr(0, Separator).c_str());
    SoftObject.ObjectID.AssetPath.AssetName = FName(Path.substr(Separator + 1).c_str());
    SoftObject.WeakPtr = FWeakObjectPtr(FindObject<UObject>(Path.c_str()));
}

static bool IsUsingAISettings(UAthenaAIServicePlayerBots* Service, const wchar_t* AISettingsPath)
{
    auto GameMode = Service->CachedGameMode;

    return GameMode && GameMode->AISettings && GameMode->AISettings == StaticFindObject(AISettingsPath, UAthenaAISettings::StaticClass());
}

static int32 GetPlaylistMaxPlayerCount(UAthenaAIServicePlayerBots* Service)
{
    return ((int32 (*)(UAthenaAIServicePlayerBots*))Service->VTable[0x64])(Service);
}

static void OnPlaylistLoaded(UAthenaAIServicePlayerBots* Service)
{
    static auto IsBotEnabled = (bool (*)(UWorld*, bool))(ImageBase + 0xA8D2534);

    auto ManagerData = FindObject<UCurveTable>(L"/Game/Athena/AI/Phoebe/ManagerData/Phoebe_ManagerData.Phoebe_ManagerData");
    auto PhoebeData = FindObject<UCurveTable>(L"/Game/Athena/AI/Phoebe/DataTables/Phoebe_Data.Phoebe_Data");
    auto DataByMaxSquadSize = FindObject<UCurveTable>(L"/Game/Athena/AI/Phoebe/DataTables/Phoebe_DataByMaxSquadSize.Phoebe_DataByMaxSquadSize");
    auto DataBySafeZoneIndex = FindObject<UCurveTable>(L"/Game/Athena/AI/Phoebe/DataTables/Phoebe_DataBySafeZoneIndex.Phoebe_DataBySafeZoneIndex");
    auto bClyde = IsUsingAISettings(Service, L"/Clyde/Playlist/PlayerBotsFlow_AISettings_Clyde.PlayerBotsFlow_AISettings_Clyde");
    auto bBlastBerry = IsUsingAISettings(Service, L"/BlastBerry/AI/Bots/PlayerBotsFlow_AISettings_BlastBerry.PlayerBotsFlow_AISettings_BlastBerry");
    auto NextPOIData = bBlastBerry ? FindObject<UCurveTable>(L"/BlastBerry/AI/Bots/Data/Phoebe_Data_BlastBerry.Phoebe_Data_BlastBerry") : PhoebeData;

    Service->GamePhaseToStartSpawning = EAthenaGamePhase::Warmup;

    if (!bNavCacheAttached)
        Service->bWaitForNavmeshToBeLoaded = false;

    Service->DefaultBotAISpawnerData = FindObject<UClass>(bBlastBerry ? L"/BlastBerry/AI/Bots/SpawnData/BP_AISD_Phoebe_BlastBerry.BP_AISD_Phoebe_BlastBerry_C" : L"/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C");
    Service->BotItemDataTable = FindObject<UDataTable>(bClyde ? L"/Clyde/Datatables/Phoebe/Clyde_CompositePhoebeItemData.Clyde_CompositePhoebeItemData" : L"/Game/Athena/AI/Phoebe/ManagerData/PhoebeItemData.PhoebeItemData");
    Service->VisibilitySettingsRegistry = FindObject<UDataRegistry>(L"/Game/DataRegistries/AI/Phoebe_VisibilityRegistry.Phoebe_VisibilityRegistry");
    Service->BotPOIExcludedZonesDataRegistry = FindObject<UDataRegistry>(L"/Game/DataRegistries/AI/Phoebe_DynamicPOI_ExcludedZones.Phoebe_DynamicPOI_ExcludedZones");
    Service->WeaponModRegistry = FindObject<UDataRegistry>(L"/Game/DataRegistries/AI/Phoebe_WeaponModRegistry.Phoebe_WeaponModRegistry");

    SetSoftObject(Service->MMRSpawnTable, bBlastBerry ? L"/BlastBerry/AI/Bots/Data/PhoebeMMRSpawning_BlastBerry.PhoebeMMRSpawning_BlastBerry" : L"/Game/Athena/AI/Phoebe/PhoebeSpawningTables/PhoebeMMRSpawning.PhoebeMMRSpawning");
    SetSoftObject(Service->EQSMapSampling, L"/Game/Athena/AI/Phoebe/EQS/EQS_Phoebe_BaseNavigation.EQS_Phoebe_BaseNavigation");
    SetSoftObject(Service->BotPOIEQS, L"/Game/Athena/AI/Phoebe/EQS/EQS_Phoebe_SystemicPOI.EQS_Phoebe_SystemicPOI");
    SetSoftObject(Service->FindLocationsAroundPOIQuery, L"/Game/Athena/AI/Phoebe/EQS/EQS_Phoebe_FindLocationInPOI.EQS_Phoebe_FindLocationInPOI");
    SetSoftObject(Service->BotNameDataAsset, L"/Game/Athena/AI/Phoebe/Names/PhoebeNameDataAsset.PhoebeNameDataAsset");

    SetCurve(Service->UseCustomSupportedItemList, ManagerData, L"phoebe.UseCustomSupportedItemList");
    SetCurve(Service->UseRegionalNameList, ManagerData, L"phoebe.UseRegionalNameList");
    SetCurve(Service->UseAllBattleBusPOIsSquadRatio, ManagerData, L"Phoebe.UseAllBattleBusPOIsSquadRatio");
    SetCurve(Service->ThankBusDriverProbability, ManagerData, L"Phoebe.ThankBusDriverProbability");
    SetCurve(Service->ThankBusDriverMinTime, ManagerData, L"Phoebe.ThankBusDriverMinTime");
    SetCurve(Service->ThankBusDriverMaxTime, ManagerData, L"Phoebe.ThankBusDriverMaxTime");
    SetCurve(Service->MaxNumberOfBotsToSpawnAroundPlayer, ManagerData, L"Phoebe.MaxBotsSpawnAroundPlayer");
    SetCurve(Service->BotsUniqueIDUseValidAccountID, ManagerData, L"Phoebe.BotsUniqueIDUseValidAccountID");
    SetCurve(Service->ZiplineOctreeRadius, ManagerData, L"Phoebe.AIService.ZiplineOctreeRadius");
    SetCurve(Service->UseZiplines, ManagerData, L"Phoebe.AIService.UseZiplines");
    SetCurve(Service->MaxAnonymousBotCount, ManagerData, L"Phoebe.AIService.MaxAnonymousBotCount");
    SetCurve(Service->ShowSeasonLevelProbability, ManagerData, L"Phoebe.AIService.ShowSeasonLevelProbability");
    SetCurve(Service->SpectateOthersOnDeath, ManagerData, L"Phoebe.AIService.SpectateOthersOnDeath");
    SetCurve(Service->TriggerEndGameBehaviorMinTime, ManagerData, L"Phoebe.AIService.TriggerEndGameBehaviorMinTime");
    SetCurve(Service->TriggerEndGameBehaviorMaxTime, ManagerData, L"Phoebe.AIService.TriggerEndGameBehaviorMaxTime");

    SetCurve(Service->DynamicBotPOIEnabled, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.Enabled");
    SetCurve(Service->JumpOffBusWithDynamicPOIEnabled, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.JumpOffBusWithDynamicPOIEnabled");
    SetCurve(Service->NavigationWithDynamicPOIEnabled, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.NavigationWithDynamicPOIEnabled");
    SetCurve(Service->DenyJumpBusBotPOIByProximityEnabled, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.DenyJumpBusBotPOIByProximity.Enabled");
    SetCurve(Service->BotPOINextPOIDurationSecondByWeight, NextPOIData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOINextPOIDurationSecondByWeight");
    SetCurve(Service->BotPOINextPOIMinDuration, NextPOIData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOINextPOIMinDuration");
    SetCurve(Service->BotPOINextPOIMaxDuration, NextPOIData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOINextPOIMaxDuration");
    SetCurve(Service->BotPOIScoringWeightFactor, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOIScoringWeightFactor");
    SetCurve(Service->BotPOIScoringDistanceFactor, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOIScoringDistanceFactor");
    SetCurve(Service->BotPOIScoringOccupancyFactor, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOIScoringOccupancyFactor");
    SetCurve(Service->BotPOIScoringVisitedFactor, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.BotPOIScoringVisitedFactor");
    SetCurve(Service->FailToReachBotPOICountMax, PhoebeData, L"Phoebe.ServicePlayerBots.DynamicBotPOI.FailToReachBotPOICountMax");

    SetCurve(Service->BotPOIJumpMainPOICountMax, DataByMaxSquadSize, L"Phoebe.BotPOI.JumpBus.MainPOICount");
    SetCurve(Service->BotPOISquadJumpOnMainPOICountMax, DataByMaxSquadSize, L"Phoebe.BotPOI.JumpBus.SquadJumpOnMainCount");
    SetCurve(Service->BotPOIMinimumWeightForSquadMembers, DataByMaxSquadSize, L"Phoebe.BotPOI.MinWeightToUsePOI");
    SetCurve(Service->DenyJumpBusBotPOIDistance, DataByMaxSquadSize, L"Phoebe.BotPOI.DenyJumpBusBotPOIByProximity.Distance");

    SetCurve(Service->ChanceToUseNextSafeZone, DataBySafeZoneIndex, L"Phoebe.ServicePlayerBots.DynamicBotPOI.ChanceToUseNextSafeZone");

    OnPlaylistLoadedOG(Service);

    if ((*(uint8*)(uint64(Service) + 0x70) & 1) && IsBotEnabled(UWorld::GetWorld(), false))
        PreloadBotCosmetics(Service->DefaultBotAISpawnerData, GetPlaylistMaxPlayerCount(Service));
}

static void ResetSpawningInfos(TArray<FMMRSpawningInfo>& SpawningInfos, int32 Num)
{
    static auto DestructItems = (void (*)(FMMRSpawningInfo*, int32))(ImageBase + 0xA8BF504);
    static auto ResizeForCopy = (void (*)(TArray<FMMRSpawningInfo>*, int32, int32))(ImageBase + 0x5C2BB8C);

    DestructItems(SpawningInfos.Data, SpawningInfos.NumElements);
    SpawningInfos.NumElements = 0;
    ResizeForCopy(&SpawningInfos, Num, SpawningInfos.MaxElements);
    SpawningInfos.NumElements = Num;
    memset(SpawningInfos.Data, 0, Num * sizeof(FMMRSpawningInfo));
}

static void CalculateSpawnAmountAndSkill(UAthenaAIServicePlayerBots* Service)
{
    auto Base = uint64(Service);
    auto& Flags = *(uint8*)(Base + 0xC58);

    if (Flags & 0x8)
        return;

    auto Info = uint64(&Service->CachedMMRSpawningInfo);
    auto& SpawningInfos = Service->CachedMMRSpawningInfo.SpawningInfos;
    auto TotalWeight = *(int32*)(Info + 0x30);
    auto& TotalAmountToSpawn = *(int32*)(Info + 0x34);

    if (*(int32*)(Info + 0x8) <= -99999 || *(int32*)(Info + 0x8) >= *(int32*)(Info + 0xC) || SpawningInfos.Num() <= 0 || TotalWeight <= 0)
    {
        Flags |= 0x8;
        return;
    }

    auto& AmountToSpawn = *(int32*)(Base + 0xD84);

    if (AmountToSpawn == -1)
    {
        AmountToSpawn = *(int32*)(Base + 0xD88);

        if (AmountToSpawn == -1)
        {
            AmountToSpawn = 0;

            if (Service->CachedGameMode && Service->CachedGameState)
            {
                auto MaxPlayers = GetPlaylistMaxPlayerCount(Service);
                auto MaxSpawnAmount = *(int32*)(ImageBase + 0x12C6281C);

                AmountToSpawn = (int32)*(float*)(ImageBase + 0x12C62824);

                if (AmountToSpawn <= 0)
                    AmountToSpawn = MaxPlayers > *(int32*)(Base + 0xD80) ? MaxPlayers - *(int32*)(Base + 0xD80) : 0;

                if (MaxSpawnAmount > 0 && AmountToSpawn > MaxSpawnAmount)
                    AmountToSpawn = MaxSpawnAmount;
            }
        }
    }

    TotalAmountToSpawn = 0;

    for (int32 i = 0; i < SpawningInfos.Num(); i++)
    {
        auto Entry = uint64(&SpawningInfos.Data[i]);
        auto& NumBotsToSpawn = *(int32*)(Entry + 0x8);

        NumBotsToSpawn = (int32)((float)AmountToSpawn * (*(float*)(Entry + 0x4) / (float)TotalWeight));
        TotalAmountToSpawn += NumBotsToSpawn;
    }

    for (int32 i = 0; i < SpawningInfos.Num() && TotalAmountToSpawn < AmountToSpawn; i++)
    {
        (*(int32*)(uint64(&SpawningInfos.Data[i]) + 0x8))++;
        TotalAmountToSpawn++;
    }

    if (TotalAmountToSpawn != AmountToSpawn)
        TotalAmountToSpawn = 0;

    Flags |= 0x8;
}

static void UseFailSafeMMRInfo(UAthenaAIServicePlayerBots* Service, float TargetELO)
{
    auto Info = uint64(&Service->CachedMMRSpawningInfo);
    auto& SpawningInfos = Service->CachedMMRSpawningInfo.SpawningInfos;

    *(int32*)(Info + 0x8) = -1000;
    *(int32*)(Info + 0xC) = 4000;
    *(int32*)(Info + 0x30) = 100;

    ResetSpawningInfos(SpawningInfos, 1);

    auto Entry = uint64(&SpawningInfos.Data[0]);

    *(float*)(Entry + 0x4) = 100.f;
    *(float*)Entry = TargetELO > -1.f ? TargetELO : 1100.f;
    *(UClass**)(Entry + 0x10) = Service->DefaultBotAISpawnerData;

    CalculateSpawnAmountAndSkill(Service);
}

static void InitializeMMRInfos(UAthenaAIServicePlayerBots* Service)
{
    auto MMR = *(bool*)(ImageBase + 0x130DCA9A) ? *(int32*)(ImageBase + 0x12C5D8A4) : -99999;
    auto MMRSpawnTable = Service->MMRSpawnTable.Get();

    if (MMRSpawnTable)
    {
        for (auto& Pair : MMRSpawnTable->RowMap)
        {
            auto Row = (FMMRSpawningBracketBotDataTableRow*)Pair.Value();

            if (!Row || MMR < Row->MMRBracketLow || MMR > Row->MMRBracketHigh || Row->PhaseSpawningItems.Num() == 0)
                continue;

            auto Info = uint64(&Service->CachedMMRSpawningInfo);
            auto& SpawningInfos = Service->CachedMMRSpawningInfo.SpawningInfos;
            auto& SpawningItems = Row->PhaseSpawningItems.Data[0].SpawningItems;
            float TotalWeight = 0.f;

            *(FMMRSpawningBracketBotDataTableRow**)(uint64(Service) + 0xE00) = Row;
            *(int32*)(Info + 0x8) = Row->MMRBracketLow;
            *(int32*)(Info + 0xC) = Row->MMRBracketHigh;

            ResetSpawningInfos(SpawningInfos, SpawningItems.Num());

            for (int32 i = 0; i < SpawningItems.Num(); i++)
            {
                auto Entry = uint64(&SpawningInfos.Data[i]);
                auto& Item = SpawningItems.Data[i];

                *(float*)Entry = Item.Skill;
                *(float*)(Entry + 0x4) = Item.Weight;
                *(UClass**)(Entry + 0x10) = Item.AISpawnerData;
                TotalWeight += Item.Weight;
            }

            *(int32*)(Info + 0x30) = (int32)TotalWeight;

            return CalculateSpawnAmountAndSkill(Service);
        }
    }

    UseFailSafeMMRInfo(Service, (float)MMR);
}

static void StartSpawning(UAthenaAIServicePlayerBots* Service, void* FlowNode)
{
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    if (!bPlayerBots && GameMode)
    {
        GameMode->PlaylistEnableBots.Value = 0.f;
        GameMode->PlaylistEnableBots.Curve.CurveTable = nullptr;
        GameMode->PlaylistEnableBots.Curve.RowName = FName();
    }

    InitializeMMRInfos(Service);
    StartSpawningOG(Service, FlowNode);

    if (*(uint64*)(uint64(Service) + 0xE40))
        PauseWarmup(EGameModePauseWarmupMaskId::PlayerBotsSpawning);
}

static void UnpauseWarmUp(UAthenaAIServicePlayerBots* Service)
{
    UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(Service, *(FTimerHandle*)(uint64(Service) + 0xE40));
    UnPauseWarmup(EGameModePauseWarmupMaskId::PlayerBotsSpawning);
}

static void OnAllBotsSpawned(UAthenaAIServicePlayerBots* Service)
{
    OnAllBotsSpawnedOG(Service);
    UnpauseWarmUp(Service);
}

static void UnpauseWarmupFailSafe(UAthenaAIServicePlayerBots* Service)
{
    UnpauseWarmupFailSafeOG(Service);

    if (!*(uint64*)(uint64(Service) + 0xE40))
        UnpauseWarmUp(Service);
}

static void FetchPlayerBotNameAndID(FString* OutBotName, FString* OutBotAccountID, AFortAthenaAIBotController* Controller)
{
    static auto Empty = (void (*)(FString*))(ImageBase + 0x4B6DDF0);
    static auto AssignRange = (void (*)(FString*, const wchar_t*, int32))(ImageBase + 0x18B0C38);
    static auto Emplace = (void* (*)(void*, int32*, const FString*, bool*))(ImageBase + 0x186D770);
    static auto AsBool = (bool (*)(const FScalableFloat*, float))(ImageBase + 0x1EECCB4);
    static auto GetQosInterface = (void** (*)(void**))(ImageBase + 0x2C7E224);
    static auto GetRegionId = (FString* (*)(void*, FString*))(ImageBase + 0x2C7E040);
    static auto ReleaseQosInterface = (void (*)(void**))(ImageBase + 0x14EAAD8);

    auto Service = Controller->CachedAIServicePlayerBots;
    auto NameDataAsset = Service->BotNameDataAsset.Get();

    Empty(OutBotName);
    Empty(OutBotAccountID);

    if (!NameDataAsset)
        return;

    auto NameDataTable = NameDataAsset->DefaultNameDataTable.ObjectID.AssetPath.AssetName;

    if (AsBool(&Service->UseRegionalNameList, 0.f))
    {
        void* QosInterface[2] = {};
        FString RegionId;

        GetRegionId(*GetQosInterface(QosInterface), &RegionId);
        ReleaseQosInterface(QosInterface);

        for (int32 i = 0; i < NameDataAsset->RegionData.Num(); i++)
        {
            auto& Region = NameDataAsset->RegionData[i];

            if (RegionId.Num() && Region.RegionId.Num() && !_wcsicmp(Region.RegionId.CStr(), RegionId.CStr()))
            {
                NameDataTable = Region.NameDataTable.ObjectID.AssetPath.AssetName;
                break;
            }
        }

        Empty(&RegionId);
    }

    auto BotNames = PhoebeNames.find(NameDataTable.ToString());

    if (BotNames == PhoebeNames.end() || BotNames->second.empty())
        return;

    auto& Names = BotNames->second;
    auto Count = int32(Names.size());
    auto Start = rand() % Count;

    for (int32 i = 0; i < Count; i++)
    {
        auto Name = Names[(Start + i) % Count];
        auto Length = int32(wcslen(Name));
        FString Candidate((wchar_t*)Name, Length + 1, Length + 1);
        int32 ElementId;
        bool bAlreadyInSet = false;

        Emplace(&Service->ReservedPlayerNames, &ElementId, &Candidate, &bAlreadyInSet);

        if (bAlreadyInSet)
            continue;

        AssignRange(OutBotName, Name, Length);
        return;
    }
}

void FinishPendingRespawns(UAthenaAIServicePlayerBots* Service)
{
    static auto RespawnPlayerAfterDeath = (void (*)(AFortAthenaAIBotController*))(ImageBase + 0xA795818);

    std::vector<AFortAthenaAIBotController*> RespawningBots;

    for (int32 i = 0; i < Service->PlayerBots.Num(); i++)
    {
        auto Controller = Service->PlayerBots.Data[i].BotController;

        if (Controller && *(uint64*)(uint64(Controller) + 0xC68))
            RespawningBots.push_back(Controller);
    }

    for (auto Controller : RespawningBots)
        RespawnPlayerAfterDeath(Controller);
}

void UnspawnBotFromWarmup(UAthenaAIServicePlayerBots* Service)
{
    static auto EquipWeapon = (bool (*)(AFortPawn*, AFortWeapon*))(ImageBase + 0xA145620);
    static auto SetIsInAnyStorm = (void (*)(AFortPlayerPawn*, bool))(ImageBase + 0xA19FE60);
    static auto ProcessMulticastDelegate = (void (*)(void*, void*))(ImageBase + 0x214A62C);
    static auto UnPossess = (void (*)(AController*))(ImageBase + 0x305F53C);
    static auto Destroy = (bool (*)(AActor*, bool, bool))(ImageBase + 0x2878B04);
    static auto ResizeGrow = (void (*)(TArray<FFortServerBotInfo>*, int32))(ImageBase + 0x5C9BCE0);

    if (!Service->CachedAIPopulationTracker)
        return;

    for (int32 i = 0; i < Service->PlayerBots.Num(); i++)
    {
        if (auto Controller = Service->PlayerBots.Data[i].BotController)
        {
            if (auto StartupInventory = Controller->StartupInventory)
            {
                auto InventoryOwner = (void*)(uint64(Controller) + 0x3F0);

                ((void (*)(void*, TArray<FItemAndCount>*))(*(void***)InventoryOwner)[0x4])(InventoryOwner, &StartupInventory->Items);
            }

            if (auto Pawn = Controller->PlayerBotPawn)
            {
                EquipWeapon(Pawn, nullptr);
                SetIsInAnyStorm(Pawn, false);
                ((void (*)(AFortPlayerPawnAthena*, bool))Pawn->VTable[0x271])(Pawn, true);
                ProcessMulticastDelegate(&Pawn->OnEnteredAircraft, nullptr);
                UnPossess(Controller);
                Destroy(Pawn, false, true);
            }
        }

        auto& PlayerBotsRestartLeft = Service->PlayerBotsRestartLeft;
        auto Num = PlayerBotsRestartLeft.NumElements;

        PlayerBotsRestartLeft.NumElements = Num + 1;

        if (PlayerBotsRestartLeft.NumElements > PlayerBotsRestartLeft.MaxElements)
            ResizeGrow(&PlayerBotsRestartLeft, Num);

        PlayerBotsRestartLeft.Data[Num] = Service->PlayerBots.Data[i];
    }
}

void PlacePlayerBotsInAircraft(UAthenaAIServicePlayerBots* Service, UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic)
{
    static auto IsBotEnabled = (bool (*)(UWorld*, bool))(ImageBase + 0xA8D2534);
    static auto GetAircraftIndex = (int32 (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, APlayerState*))(ImageBase + 0x1E35610);
    static auto GetAircraft = (AFortAthenaAircraft* (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, int32))(ImageBase + 0x1E34810);
    static auto ComputePOISquadRepartition = (void (*)(UAthenaAIServicePlayerBots*))(ImageBase + 0xA8C8C44);

    if (!IsBotEnabled(UWorld::GetWorld(), false) || !Service->CachedGameState || !Service->CachedAIPopulationTracker)
        return;

    for (int32 i = 0; i < Service->PlayerBots.Num(); i++)
    {
        auto Controller = Service->PlayerBots.Data[i].BotController;

        if (!Controller)
            continue;

        auto AircraftIndex = GetAircraftIndex(GamePhaseLogic, Controller->PlayerState);

        if (AircraftIndex >= 0 && AircraftIndex < GamePhaseLogic->Aircrafts_GameState.Num())
            EnterAircraft(Controller, GetAircraft(GamePhaseLogic, AircraftIndex));
    }

    ComputePOISquadRepartition(Service);
}

void ForceAllExitAircraft(UAthenaAIServicePlayerBots* Service)
{
    if (!Service->CachedAIPopulationTracker)
        return;

    for (int32 i = 0; i < Service->PlayerBots.Num(); i++)
    {
        auto Controller = Service->PlayerBots.Data[i].BotController;

        if (Controller && !Controller->PlayerBotPawn)
            ExitAircraft(Controller);
    }
}

static void AssignBotTeam(UAthenaAIServicePlayerBots* Service, AFortAthenaAIBotController* Controller, AFortTeamInfo* TeamInfo, uint8 SquadId)
{
    static auto FindOrAddGroup = (void* (*)(void*, uint32))(ImageBase + 0x27E13F8);
    static auto AddToGroup = (void (*)(void*, APlayerState*))(ImageBase + 0x27E144C);
    static auto BroadcastGroupMemberAdded = (void (*)(void*, uint8, uint8, APlayerState*))(ImageBase + 0x33C14A0);

    AssignBotTeamOG(Service, Controller, TeamInfo, SquadId);

    auto PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
    auto GameState = UWorld::GetWorld()->GameState;

    if (!Controller->bIsAnAthenaGameParticipant || !PlayerState || !GameState)
        return;

    AddToGroup(FindOrAddGroup((void*)(uint64(GameState) + 0x1A30), PlayerState->TeamIndex), PlayerState);
    AddToGroup(FindOrAddGroup((void*)(uint64(GameState) + 0x1A68), PlayerState->SquadID), PlayerState);
    BroadcastGroupMemberAdded((void*)(uint64(GameState) + 0x1C60), PlayerState->SquadID, PlayerState->TeamIndex, PlayerState);
}

void UAthenaAIServicePlayerBots__Init()
{
    Hook<UAthenaAIServicePlayerBots>(uint32(0x59), OnPlaylistLoaded, OnPlaylistLoadedOG);
    Hook(ImageBase + 0xA8E2FDC, StartSpawning, StartSpawningOG);
    Hook(ImageBase + 0xA8D5C90, OnAllBotsSpawned, OnAllBotsSpawnedOG);
    Hook(ImageBase + 0xA8E47EC, UnpauseWarmupFailSafe, UnpauseWarmupFailSafeOG);
    Hook(ImageBase + 0xA8C41F8, AssignBotTeam, AssignBotTeamOG);

    int32 i = 0;

    for (auto Byte : { 0x48, 0x8D, 0x54, 0x24, 0x40, 0x4C, 0x8B, 0xC6, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90 })
        Patch<uint8_t>(ImageBase + 0xA771812 + i++, uint8_t(Byte));

    Rel32Hook(ImageBase + 0xA77181A + 1, FetchPlayerBotNameAndID);
}
