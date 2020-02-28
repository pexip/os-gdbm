// Copyright 2018 Dmitry Bogatov <KAction@gnu.org>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include <gdbm.h>
#include <stdlib.h>
#include <stdio.h>

static const int magic = 0xABCD;

int
main(int argc, char **argv)
{
	const char *filename;
	GDBM_FILE db;
	datum key, value;

	if (argc != 2)
		return 1;
	filename = argv[1];
	db = gdbm_open(argv[1], 0, GDBM_NEWDB, 0777, NULL);
	if (!db) {
		fputs(gdbm_strerror(gdbm_errno), stderr);
		return 1;
	}

	key.dptr = &magic;
	key.dsize = sizeof(magic);
	value = key;
	gdbm_store(db, key, value, GDBM_REPLACE);
	gdbm_close(db);

	db = gdbm_open(argv[1], 0, GDBM_READER, 0777, NULL);
	if (!db) {
		fputs(gdbm_strerror(gdbm_errno), stderr);
		return 1;
	}
	value = gdbm_fetch(db, key);
	if (!value.dptr || *(int *) value.dptr != magic) {
		fputs("Invalid value extracted", stderr);
		return 1;
	}
	free(value.dptr);
	gdbm_close(db);
	return 0;
}
