/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamecontrol.cpp High level game control code. */

#include "stdafx.h"
#include "gamecontrol.h"
#include "finances.h"
#include "sprite_store.h"
#include "person.h"
#include "people.h"
#include "window.h"
#include "dates.h"
#include "viewport.h"
#include "weather.h"
#include "freerct.h"

GameModeManager _game_mode_mgr; ///< Game mode manager object.

/** Runs various procedures that have to be done yearly. */
void OnNewYear()
{
	// Nothing (yet) needed.
}

/** Runs various procedures that have to be done monthly. */
void OnNewMonth()
{
	_finances_manager.AdvanceMonth();
	_rides_manager.OnNewMonth();
}

/** Runs various procedures that have to be done daily. */
void OnNewDay()
{
	_rides_manager.OnNewDay();
	_guests.OnNewDay();
	_weather.OnNewDay();
	NotifyChange(WC_BOTTOM_TOOLBAR, ALL_WINDOWS_OF_TYPE, CHG_DISPLAY_OLD, 0);
}

/**
 * For every frame do...
 * @param frame_delay Number of milliseconds between two frames.
*/
void OnNewFrame(uint32 frame_delay)
{
	_window_manager.Tick();
	_guests.DoTick();
	_date.OnTick();
	_guests.OnAnimate(frame_delay);
	_rides_manager.OnAnimate(frame_delay);
}

GameControl::GameControl()
{
	this->running = false;
	this->next_action = GCA_NONE;
	this->fname = "";
}

GameControl::~GameControl()
{
}

/** Initialize the game controller. */
void GameControl::Initialize()
{
	this->running = true;
	this->NewGame();
	this->RunAction();
}

/** Uninitialize the game controller. */
void GameControl::Uninitialize()
{
	this->ShutdownLevel();
}

/**
 * Run latest game control action.
 * @pre next_action should not be equal to #GCA_NONE.
 */
void GameControl::RunAction()
{
	switch (this->next_action) {
		case GCA_NEW_GAME:
		case GCA_LOAD_GAME:
			this->ShutdownLevel();

			if (this->next_action == GCA_NEW_GAME) {
				this->NewLevel();
			} else {
				LoadGameFile(this->fname.c_str());
			}

			this->StartLevel();
			break;

		case GCA_SAVE_GAME:
			SaveGameFile(this->fname.c_str());
			break;

		case GCA_QUIT:
			this->running = false;
			break;

		default:
			NOT_REACHED();
	}

	this->next_action = GCA_NONE;
}

/** Prepare for a #GCA_NEW_GAME action. */
void GameControl::NewGame()
{
	this->next_action = GCA_NEW_GAME;
}

/**
 * Prepare for a #GCA_LOAD_GAME action.
 * @param fname Name of the file to load.
 */
void GameControl::LoadGame(const std::string &fname)
{
	this->fname = fname;
	this->next_action = GCA_LOAD_GAME;
}

/**
 * Prepare for a #GCA_SAVE_GAME action.
 * @param fname Name of the file to write.
 */
void GameControl::SaveGame(const std::string &fname)
{
	this->fname = fname;
	this->next_action = GCA_SAVE_GAME;
}

/** Prepare for a #GCA_QUIT action. */
void GameControl::QuitGame()
{
	this->next_action = GCA_QUIT;
}

/** Initialize all game data structures for playing a new game. */
void GameControl::NewLevel()
{
	/// \todo We blindly assume game data structures are all clean.
	_world.SetWorldSize(20, 21);
	_world.MakeFlatWorld(8);
	_world.SetTileOwnerGlobally(OWN_NONE);
	_world.SetTileOwnerRect(2, 2, 16, 15, OWN_PARK);
	_world.SetTileOwnerRect(8, 0, 4, 2, OWN_PARK); // Allow building path to map edge in north west.
	_world.SetTileOwnerRect(2, 18, 16, 2, OWN_FOR_SALE);

	_finances_manager.SetScenario(_scenario);
	_weather.Initialize();
}

/** Initialize common game settings and view. */
void GameControl::StartLevel()
{
	_game_mode_mgr.SetGameMode(GM_PLAY);

	XYZPoint32 view_pos(_world.GetXSize() * 256 / 2, _world.GetYSize() * 256 / 2, 8 * 256);
	ShowMainDisplay(view_pos);
	ShowToolbar();
	ShowBottomToolbar();
}

/** Shutdown the game interaction. */
void GameControl::ShutdownLevel()
{
	/// \todo Clean out the game data structures.
	_guests.Uninitialize();
	_game_mode_mgr.SetGameMode(GM_NONE);
	_window_manager.CloseAllWindows();
}

GameModeManager::GameModeManager()
{
	this->game_mode = GM_NONE;
}

GameModeManager::~GameModeManager()
{
}

/**
 * Change game mode of the program.
 * @param new_mode New mode to use.
 */
void GameModeManager::SetGameMode(GameMode new_mode)
{
	this->game_mode = new_mode;
	NotifyChange(WC_TOOLBAR, 0, CHG_UPDATE_BUTTONS, 0);
}
