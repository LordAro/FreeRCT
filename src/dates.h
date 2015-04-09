/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dates.h Declarations for dates in the game. */

#ifndef DATES_H
#define DATES_H

static const int TICK_COUNT_PER_DAY = 300; ///< Number of ticks in a day (stored in #Date::frac).

typedef uint32 CompressedDate; ///< Compressed date for easy transfer/storage.

/** Bits and sizes of the compressed date format. */
enum CompressedDateBits {
	CDB_DAY_LENGTH   =  5, ///< Length of the 'day'      field in the compressed date.
	CDB_MONTH_LENGTH =  4, ///< Length of the 'month'    field in the compressed date.
	CDB_YEAR_LENGTH  =  7, ///< Length of the 'year'     field in the compressed date.
	CDB_FRAC_LENGTH  = 10, ///< Length of the 'fraction' field in the compressed date.

	CDB_DAY_START = 0,                                   ///< Start bit of the 'day' field.
	CDB_MONTH_START = CDB_DAY_START + CDB_DAY_LENGTH,    ///< Start bit of the 'month' field.
	CDB_YEAR_START = CDB_MONTH_START + CDB_MONTH_LENGTH, ///< Start bit of the 'year' field.
	CDB_FRAC_START = CDB_YEAR_START + CDB_YEAR_LENGTH,   ///< Start bit of the 'fraction' field.
};

/** %Date in the game. */
class Date {
public:
	Date();
	Date(int pday, int pmonth, int pyear, int pfrac = 0);
	Date(CompressedDate cd);
	Date(const Date &d) = default;
	Date &operator=(const Date &d) = default;

	CompressedDate Compress() const;

	int GetNextMonth() const;
	int GetPreviousMonth() const;

	void OnTick();
	void Load(Loader &ldr);
	void Save(Saver &svr);

	int day;   ///< Day of the month, 1-based.
	int month; ///< Month of the year, 1-based.
	int year;  ///< The current year, 1-based.
	int frac;  ///< Day fraction, 0-based.
};


extern Date _date;
extern const int _days_per_month[13];

#endif
