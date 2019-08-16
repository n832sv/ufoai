/**
 * @file
 * @brief Header file for single player campaign control.
 */

/*
Copyright (C) 2002-2019 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

extern memPool_t* cp_campaignPool;

class Employee;
#define MAX_CAMPAIGNS	16

#include "cp_cgame_callbacks.h"
#include "cp_alien_interest.h"
#include "cp_rank.h"
#include "cp_save.h"
#include "cp_parse.h"
#include "cp_event.h"
#include "cp_ufopedia.h"
#include "cp_research.h"
#include "cp_radar.h"
#include "cp_aircraft.h"
#include "cp_base.h"
#include "cp_employee.h"
#include "cp_transfer.h"
#include "cp_nation.h"
#include "cp_installation.h"
#include "cp_produce.h"
#include "cp_uforecovery.h"
#include "cp_airfight.h"
#include "cp_messageoptions.h"
#include "cp_alienbase.h"
#include "cp_market.h"
#include "cp_statistics.h"
#include "cp_component.h"

/* Maximum alien groups per alien team category */
#define MAX_ALIEN_GROUP_PER_CATEGORY	24
/* Maximum alien team category defined in scripts */
#define ALIENCATEGORY_MAX	128
#define MAX_PROJECTILESONGEOSCAPE 64

/**
 * @brief The amount of time (in hours) it takes for the interest to increase by 1. Is later affected by difficulty.
 */
#define HOURS_PER_ONE_INTEREST				24

/**
 * @brief Determines the interest interval for a single campaign
 */
#define INITIAL_OVERALL_INTEREST 			20
#define FINAL_OVERALL_INTEREST 				400

/**
 * @brief The length of a single mission spawn cycle
 */
#define DELAY_BETWEEN_MISSION_SPAWNING 		12

/**
 * @brief Determines the early game period during which DELAY_BETWEEN_MISSION_SPAWNING is halved.
 * @note This is done to ensure the player is not bored early in the game. The rush will stop when alien interest reaches this level.
 */
#define EARLY_UFO_RUSH_INTEREST 			50

/**
 * @brief The probability that any new alien mission will be a non-occurrence mission.
 */
#define NON_OCCURRENCE_PROBABILITY 			0.75

/** possible map types */
typedef enum mapType_s {
	MAPTYPE_TERRAIN,
	MAPTYPE_CULTURE,
	MAPTYPE_POPULATION,
	MAPTYPE_NATIONS,

	MAPTYPE_MAX
} mapType_t;

/** @brief alien team group definition.
 * @note This is the definition of one groups of aliens (several races) that can
 * be used on the same map.
 * @sa alienTeamCategory_s
 */
typedef struct alienTeamGroup_s {
	int idx;			/**< idx of the group in the alien team category */
	int categoryIdx;	/**< idx of category it's used in */
	int minInterest;	/**< Minimum interest value this group should be used with. */
	int maxInterest;	/**< Maximum interest value this group should be used with. */
	int minAlienCount;	/**< Minimum number of aliens in this group */
	int maxAlienCount;	/**< Maximum number of aliens in this group */

	const teamDef_t* alienTeams[MAX_TEAMS_PER_MISSION];	/**< different alien teams available
													 * that will be used in mission */
	const chrTemplate_t* alienChrTemplates[MAX_TEAMS_PER_MISSION];
	int numAlienTeams;		/**< Number of alienTeams defined in this group. */
} alienTeamGroup_t;

/** @brief alien team category definition
 * @note This is the definition of all groups of aliens that can be used for
 * a mission category
 * @sa alienTeamGroup_s
 */
typedef struct alienTeamCategory_s {
	char id[MAX_VAR];			/**< id of the category */
	interestCategory_t missionCategories[INTERESTCATEGORY_MAX];		/**< Mission category that should use this
															 * alien team Category. */
	int numMissionCategories;					/**< Number of category using this alien team Category. */

	linkedList_t* equipment;		/**< Equipment definitions that may be used for this def. */

	alienTeamGroup_t alienTeamGroups[MAX_ALIEN_GROUP_PER_CATEGORY];		/**< Different alien group available
																 * for this category */
	int numAlienTeamGroups;			/**< Number of alien group defined for this category */
} alienTeamCategory_t;

/** battlescape parameters that were used */
typedef struct battleParam_s {
	struct mission_s* mission;
	alienTeamGroup_t* alienTeamGroup;	/**< Races of aliens present in battle */
	char* param;				/**< in case of a random map assembly we can't use the param from mapDef - because
						 * this is global for the mapDef - but we need a local mission param */
	char alienEquipment[MAX_VAR];		/**< Equipment of alien team */
	char civTeam[MAX_VAR];			/**< Type of civilian (European, ...) */
	bool day;				/**< Mission is played during day */
	const char* zoneType;			/**< Terrain type (used for texture replacement in some missions (base, ufocrash)) */
	int aliens, civilians;			/**< number of aliens and civilians in that particular mission */
	struct nation_s* nation;		/**< nation where the mission takes place */
	bool retriable;				/**< if the battle can be retried */
} battleParam_t;

/** salary values for a campaign */
typedef struct salary_s {
	int base[MAX_EMPL];
	int rankBonus[MAX_EMPL];
	int aircraftFactor;
	int aircraftDivisor;
	int baseUpkeep;
	float debtInterest;
} salary_t;

/** campaign definition */
typedef struct campaign_s {
	int idx;				/**< own index in global campaign array */
	char id[MAX_VAR];			/**< id of the campaign */
	char name[MAX_VAR];			/**< name of the campaign */
	bool defaultCampaign;			/**< if this is the default campaign */
	int team;				/**< what team can play this campaign */
	char researched[MAX_VAR];		/**< name of the researched tech list to use on campaign start */
	char soldierEquipment[MAX_VAR];		/**< name of the equipment list that is used to equip soldiers on crafts that are added to the first base */
	char equipment[MAX_VAR];		/**< name of the equipment list to use on campaign start */
	char market[MAX_VAR];			/**< name of the market list containing initial items on market */
	char asymptoticMarket[MAX_VAR];		/**< name of the market list containing items on market at the end of the game */
	const equipDef_t* marketDef;		/**< market definition for this campaign (how many items on the market) containing initial items */
	const equipDef_t* asymptoticMarketDef;	/**< market definition for this campaign (how many items on the market) containing finale items */
	char text[MAX_VAR];			/**< placeholder for gettext stuff */
	char map[MAX_VAR];			/**< geoscape map */
	int soldiers;				/**< start with x soldiers */
	int scientists;				/**< start with x scientists */
	int workers;				/**< start with x workers */
	int pilots;				/**< start with x pilots */
	int ugvs;				/**< start with x ugvs (robots) */
	int credits;				/**< start with x credits */
	int num;
	signed int difficulty;			/**< difficulty level -4 - 4 */
	float minhappiness;			/**< minimum value of mean happiness before the game is lost */
	int negativeCreditsUntilLost;		/**< bankrupt - negative credits until you've lost the game */
	int maxAllowedXVIRateUntilLost;		/**< 0 - 100 - the average rate of XVI over all nations before you've lost the game */
	bool visible;				/**< visible in campaign menu? */
	date_t date;				/**< starting date for this campaign */
	int basecost;				/**< base building cost for empty base */
	char firstBaseTemplate[MAX_VAR];	/**< template to use for setting up the first base */
	bool finished;
	const campaignEvents_t* events;
	salary_t salaries;
	float produceRate;			/**< higher number = faster production */
	float researchRate;			/**< specifies the number of research hours that a single scientist produces in a single hour of game time */
	float healingRate;			/**< the hospital healing rate */
	float liquidationRate;			/**< amount of original price of a building returned when it's removed from base due to recycling */
	float componentRate;			/**< Component cost percentage to build an object in the workshop */
	int minMissions;			/**< Minimum Missions per Interest Level */
	int maxMissions;			/**< Maximum Missions per Interest Level */
	float ufoReductionRate;			/**< is used in the equation which spawns UFOs to determine the probability that a UFO will not be spawned.
							 * It works like this: the game says: 10 UFOs can be spawned in this cycle. For each UFO it picks a number between 0.0 and 1.0.
							 * If the number is higher than ufoReductionRate, it is spawned. Otherwise it is not. So higher numbers lead to fewer UFOs. */
	float employeeRate;			/**< Easier difficulties could offer more monthly recruits and harder difficulties less */
	int initialInterest;
	int alienBaseInterest;			/**< the alien interest level at which aliens begin trying to build bases. */
	linkedList_t* initialCraft;		/**< List of aircraft the player starts the campaign with */
} campaign_t;

int CP_GetSalaryUpKeepBase(const salary_t* salary, const base_t* base);

/** possible geoscape actions */
typedef enum mapAction_s {
	MA_NONE,
	MA_NEWBASE,				/**< build a new base */
	MA_NEWINSTALLATION			/**< build a new installation */
} mapAction_t;

typedef void (*missionSpawnFunction_t) (void);
typedef void (*missionResultFunction_t) (const struct missionResults_s* results);

/**
 * @brief client campaign structure
 * @sa csi_t
 */
typedef struct ccs_s {
	equipDef_t eMission;					/**< this is a copy of the base storage we are starting a mission from. This is needed because
								 * we don't want to "waste" items on a retry. */
	market_t eMarket;					/**< Prices, evolution and number of items on market */

	linkedList_t* missions;					/**< Missions spawned (visible on geoscape or not) */

	battleParam_t battleParameters;				/**< Structure used to remember every parameter used during last battle */

	int lastInterestIncreaseDelay;				/**< How many hours since last increase of alien overall interest */
	int overallInterest;					/**< overall interest of aliens: how far is the player in the campaign */
	int interest[INTERESTCATEGORY_MAX];			/**< interest of aliens: determine which actions aliens will undertake */
	int lastMissionSpawnedDelay;				/**< How many days since last mission has been spawned */

	int credits;						/**< actual credits amount */
	int civiliansKilled;					/**< how many civilians were killed already */
	int aliensKilled;					/**< how many aliens were killed already */
	date_t date;						/**< current date */
	bool startXVI;						/**< the aliens are starting to spread XVI */
	float timer;
	float frametime;

	struct {
		struct mission_s* selectedMission;			/**< Currently selected mission on geoscape */
		aircraft_t* selectedAircraft;			/**< Currently selected aircraft on geoscape */
		aircraft_t* selectedUFO;			/**< Currently selected UFO on geoscape */
		aircraft_t* interceptAircraft;			/**< selected aircraft for interceptions */
		aircraft_t* missionAircraft;			/**< aircraft pointer for mission handling */
	} geoscape;

	/* == misc == */
	/** @todo eliminate or move into Lua UI scripts */
	mapAction_t mapAction;					/**< New installation and base building status */
	vec2_t newBasePos;					/**< Coordinates to place the new base at (long, lat) */

	/* how fast the game is running */
	int gameTimeScale;
	int gameLapse;

	/* already paid in this month? */
	bool paid;

	/* == employees == */
	/* A list of all phalanx employees (soldiers, scientists, workers, etc...) */
	linkedList_t* employees[MAX_EMPL];

	/* == technologies == */
	/* A list of all research-topics resp. the research-tree. */
	technology_t technologies[MAX_TECHNOLOGIES];
	int numTechnologies;

	/* == bases == */
	/* A list of _all_ bases ... even unbuilt ones. */
	base_t bases[MAX_BASES];
	int numBases;

	/* a list of all templates for building bases */
	baseTemplate_t baseTemplates[MAX_BASETEMPLATES];
	int numBaseTemplates;

	/* == aircraft == */
	linkedList_t* aircraft;

	/* == Alien bases == */
	linkedList_t* alienBases;

	/* == Nations == */
	linkedList_t* nations;
	int numNations;

	/* == Cities == */
	linkedList_t* cities;
	int numCities;

	/* Projectiles on geoscape (during fights) */
	aircraftProjectile_t projectiles[MAX_PROJECTILESONGEOSCAPE];
	int numProjectiles;

	/* == Transfers == */
	linkedList_t* transfers;

	/* UFO components. */
	components_t components[MAX_ASSEMBLIES];
	int numComponents;

	/* == stored UFOs == */
	linkedList_t* storedUFOs;

	/* Alien Team Package used during battle */
	alienTeamCategory_t alienCategories[ALIENCATEGORY_MAX];		/**< different alien team available that will be used in mission */
	int numAlienCategories;						/** number of alien team categories defined */

	/* == ufopedia == */
	/* A list of all UFOpaedia chapters. */
	pediaChapter_t upChapters[MAX_PEDIACHAPTERS];
	/* Total number of UFOpaedia chapters */
	int numChapters;
	int numUnreadMails; /**< only for faster access (don't cycle all techs every frame) */

	eventMail_t eventMails[MAX_EVENTMAILS];	/**< holds all event mails (cl_event.c) */
	int numEventMails;	/**< how many eventmails (script-id: mail) parsed */

	campaignEvents_t campaignEvents[MAX_CAMPAIGNS];			/**< holds all campaign events (cl_event.c) */
	int numCampaignEventDefinitions;				/**< how many event definitions (script-id: events) parsed */

	campaignTriggerEvent_t campaignTriggerEvents[MAX_CAMPAIGN_TRIGGER_EVENTS];
	int numCampaignTriggerEvents;

	/* == buildings in bases == */
	/* A list of all possible unique buildings. */
	building_t buildingTemplates[MAX_BUILDINGS];
	int numBuildingTemplates;
	/*  A list of the building-list per base. (new buildings in a base get copied from buildingTypes) */
	building_t buildings[MAX_BASES][MAX_BUILDINGS];
	/* Total number of buildings per base. */
	int numBuildings[MAX_BASES];

	/* == installations == */
	/* A template for each possible installation with configurable values */
	installationTemplate_t installationTemplates[MAX_INSTALLATION_TEMPLATES];
	int numInstallationTemplates;

	/* A list of _all_ installations */
	linkedList_t* installations;

	/* UFOs on geoscape */
	aircraft_t ufos[MAX_UFOONGEOSCAPE];
	int numUFOs;	/**< The current amount of UFOS on the geoscape. */

	/* message categories */
	msgCategory_t messageCategories[MAX_MESSAGECATEGORIES];
	int numMsgCategories;

	/* entries for message categories */
	msgCategoryEntry_t msgCategoryEntries[NT_NUM_NOTIFYTYPE + MAX_MESSAGECATEGORIES];
	int numMsgCategoryEntries;

	/* == Ranks == */
	/* Global list of all ranks defined in medals.ufo. */
	rank_t ranks[MAX_RANKS];
	/* The number of entries in the list above. */
	int numRanks;

	/* cache for techdef technologies */
	technology_t* teamDefTechs[MAX_TEAMDEFS];

	/* cache for item technologies */
	technology_t* objDefTechs[MAX_OBJDEFS];

	campaign_t* curCampaign;			/**< Current running campaign */
	stats_t campaignStats;

	campaign_t campaigns[MAX_CAMPAIGNS];
	int numCampaigns;

	aircraft_t aircraftTemplates[MAX_AIRCRAFT];		/**< Available aircraft types/templates/samples. */
	int numAircraftTemplates;		/**< Number of aircraft templates. */

	missionSpawnFunction_t missionSpawnCallback;
	missionResultFunction_t missionResultCallback;

	linkedList_t* updateCharacters;
} ccs_t;

typedef struct {
	int x, y;
} screenPoint_t;

extern ccs_t ccs;
extern const int DETECTION_INTERVAL;
extern cvar_t* cp_missiontest;

#define MAX_CREDITS 100000000

#include "../cgame.h"
extern const cgame_import_t* cgi;

/* Campaign functions */
void CP_InitStartup(void);
campaign_t* CP_GetCampaign(const char* name);
void CP_CampaignInit(campaign_t* campaign, bool load);
void CP_ParseCampaignData(void);
void CP_ReadCampaignData(const campaign_t* campaign);
bool CP_IsRunning(void);

void CP_CampaignRun(campaign_t* campaign, float secondsSinceLastFrame);
void CP_CheckLostCondition(const campaign_t* campaign);
void CP_EndCampaign(bool won);

void CP_Shutdown(void);
void CP_ResetCampaignData(void);

void CP_StartSelectedMission(void);

/* Credits management */
bool CP_CheckCredits (int costs);
void CP_UpdateCredits(int credits);

bool CP_OnGeoscape(void);
