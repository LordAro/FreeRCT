/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dates.cpp Days and years in the program. */

#include "stdafx.h"
#include "dates.h"
#include "bitmath.h"
#include "gamecontrol.h"

assert_compile(TICK_COUNT_PER_DAY < (1 << CDB_FRAC_LENGTH)); ///< Day length should stay within the fraction limit.

const int _days_per_month[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; ///< Numbers of days in each 1-based month (in a non-leap year).
static const int FIRST_MONTH = 3; ///< First month in the year that the park is open, 1-based.
static const int LAST_MONTH = 9;  ///< Last month in the year that the park is open, 1-based.

Date _date; ///< %Date in the program.

/** Default constructor. */
Date::Date() : Date(1, FIRST_MONTH, 1, 0)
{
}


/**
 * Constructor od a date from a compressed date.
 * @param cd Compressed date source.
 */
Date::Date(CompressedDate cd)
	: Date(GB(cd, CDB_DAY_START,  CDB_DAY_LENGTH),  GB(cd, CDB_MONTH_START, CDB_MONTH_LENGTH),
	       GB(cd, CDB_YEAR_START, CDB_YEAR_LENGTH), GB(cd, CDB_FRAC_START,  CDB_FRAC_LENGTH))
{
}

/**
 * Constructor for a specific date.
 * @param pday Day of the month (1-based).
 * @param pmonth Month (1-based).
 * @param pyear Year (1-based).
 * @param pfrac Day fraction (0-based).
 */
Date::Date(int pday, int pmonth, int pyear, int pfrac) : day(pday), month(pmonth), year(pyear), frac(pfrac)
{
	assert(pday > 0 && pday <= _days_per_month[pmonth]);
	assert(pmonth > 0 && pmonth < 13);
	assert(pyear > 0 && pyear < (1 << CDB_YEAR_LENGTH));
	assert(pfrac >= 0 && pfrac < TICK_COUNT_PER_DAY);
}

/**
 * Compress the date to an integer number.
 * @return The compressed date.
 */
CompressedDate Date::Compress() const
{
	return (this->year << CDB_YEAR_START) | (this->month << CDB_MONTH_START) | (this->day << CDB_DAY_START) | (this->frac << CDB_FRAC_START);
}

/**
 * Get the number of the previous month that the park was open.
 * @return Month number (1-based) for the previous month.
 */
int Date::GetPreviousMonth() const
{
	if (this->month == FIRST_MONTH) return LAST_MONTH;
	return this->month - 1;
}

/**
 * Get the number of the next month that the park will be open.
 * @return Month number (1-based) for the next month.
 */
int Date::GetNextMonth() const
{
	if (this->month < LAST_MONTH) return this->month + 1;
	return FIRST_MONTH;
}

/**
 * Update the day.
 * @todo Care about leap years?
 */
void Date::OnTick()
{
	bool newmonth = false;
	bool newyear  = false;

	/* New tick. */
	this->frac++;
	if (this->frac < TICK_COUNT_PER_DAY) return;

	/* New day. */
	this->frac = 0;
	this->day++;

	/* New month. */
	if (this->day > _days_per_month[this->month]) {
		bool is_last_month = (this->month == LAST_MONTH);
		this->day = 1;
		this->month++;
		newmonth = true;

		/* New year. */
		if (is_last_month || this->month > 12) {
			this->month = FIRST_MONTH;
			this->year++;
			newyear = true;
		}
	}

	OnNewDay();
	if (newmonth) OnNewMonth();
	if (newyear)  OnNewYear();
}

/**
 * Load the current date from the save game.
 * @param ldr Input stream to load from.
 */
void Date::Load(Loader &ldr)
{
	uint32 version = ldr.OpenBlock("DATE");
	if (version == 1) {
		*this = Date(ldr.GetLong());
	} else {
		*this = Date();
		if (version != 0) ldr.SetFailMessage("Unknown date block number");
	}
	ldr.CloseBlock();
}

/**
 * Save the current date to the save game.
 * @param svr Output stream to save to.
 */
void Date::Save(Saver &svr)
{
	svr.StartBlock("DATE", 1);
	svr.PutLong(this->Compress());
	svr.EndBlock();
}
