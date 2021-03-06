/*
Miranda Database Tool
Copyright (C) 2001-2005  Richard Hughes

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

void ProcessingDone(void);

static void Finalize(time_t& ts)
{
	opts.dbChecker->Destroy();
	opts.dbChecker = nullptr;

	if (opts.hOutFile) {
		CloseHandle(opts.hOutFile);
		opts.hOutFile = nullptr;
	}

	if (errorCount && !opts.bBackup && !opts.bCheckOnly) {
		time_t dlg_ts = time(nullptr);
		if (IDYES == MessageBox(nullptr,
			TranslateT("Errors were encountered, however you selected not to backup the original database. It is strongly recommended that you do so in case important data was omitted. Do you wish to keep a backup of the original database?"),
			TranslateT("Miranda Database Tool"), MB_YESNO))
			opts.bBackup = 1;
		ts += time(nullptr) - dlg_ts;
	}

	if (opts.bBackup) {
		wchar_t dbPath[MAX_PATH], dbFile[MAX_PATH];
		mir_wstrcpy(dbPath, opts.filename);
		wchar_t* str2 = wcsrchr(dbPath, '\\');
		if (str2 != nullptr) {
			mir_wstrcpy(dbFile, str2 + 1);
			*str2 = 0;
		}
		else {
			mir_wstrcpy(dbFile, dbPath);
			dbPath[0] = 0;
		}
		for (int i = 1;; i++) {
			if (i == 1)
				mir_snwprintf(opts.backupFilename, TranslateT("%s\\Backup of %s"), dbPath, dbFile);
			else
				mir_snwprintf(opts.backupFilename, TranslateT("%s\\Backup (%d) of %s"), dbPath, i, dbFile);
			if (_waccess(opts.backupFilename, 0) == -1) break;
		}

		if (!MoveFile(opts.filename, opts.backupFilename))
			AddToStatus(STATUS_WARNING, TranslateT("Unable to rename original file"));
	}
	else if (!opts.bCheckOnly)
		if (!DeleteFile(opts.filename))
			AddToStatus(STATUS_WARNING, TranslateT("Unable to delete original file"));

	if (!opts.bCheckOnly)
		if (!MoveFile(opts.outputFilename, opts.filename))
			AddToStatus(STATUS_WARNING, TranslateT("Unable to rename output file"));
}

void __cdecl WorkerThread(void *)
{
	int task, firstTime;
	time_t ts = time(nullptr);

	AddToStatus(STATUS_MESSAGE, TranslateT("Database worker thread activated"));

	mir_wstrcpy(opts.workingFilename, opts.filename);

	if (opts.bCheckOnly) {
		mir_wstrcpy(opts.outputFilename, TranslateT("<check only>"));
		opts.hOutFile = INVALID_HANDLE_VALUE;
	}
	else {
		mir_wstrcpy(opts.outputFilename, opts.filename);
		*wcsrchr(opts.outputFilename, '.') = 0;
		mir_wstrcat(opts.outputFilename, TranslateT(" (Output).dat"));
		opts.hOutFile = CreateFile(opts.outputFilename, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (opts.hOutFile == INVALID_HANDLE_VALUE) {
			AddToStatus(STATUS_FATAL, TranslateT("Can't create output file (%u)"), GetLastError());
			ProcessingDone();
			return;
		}
	}

	DWORD sp = 0;
	firstTime = 0;

	DBCHeckCallback callback;
	callback.cbSize = sizeof(callback);
	callback.spaceUsed = 1;
	callback.spaceProcessed = 0;
	callback.pfnAddLogMessage = AddToStatus;
	callback.hOutFile = opts.hOutFile;
	callback.bCheckOnly = opts.bCheckOnly;
	callback.bBackup = opts.bBackup;
	callback.bAggressive = opts.bAggressive;
	callback.bEraseHistory = opts.bEraseHistory;
	callback.bMarkRead = opts.bMarkRead;
	callback.bConvertUtf = opts.bConvertUtf;
	opts.dbChecker->Start(&callback);

	for (task = 0;;) {
		if (callback.spaceProcessed / (callback.spaceUsed / 1000 + 1) > sp) {
			sp = callback.spaceProcessed / (callback.spaceUsed / 1000 + 1);
			SetProgressBar(sp);
		}
		WaitForSingleObject(hEventRun, INFINITE);
		if (WaitForSingleObject(hEventAbort, 0) == WAIT_OBJECT_0) {
			AddToStatus(STATUS_FATAL, TranslateT("Processing aborted by user"));
			break;
		}

		int ret = opts.dbChecker->CheckDb(task, firstTime);
		firstTime = 0;
		if (ret == ERROR_OUT_OF_PAPER) {
			Finalize(ts);
			AddToStatus(STATUS_MESSAGE, TranslateT("Elapsed time: %d sec"), time(nullptr) - ts);
			if (errorCount)
				AddToStatus(STATUS_SUCCESS, TranslateT("All tasks completed but with errors (%d)"), errorCount);
			else
				AddToStatus(STATUS_SUCCESS, TranslateT("All tasks completed successfully"));
			break;
		}
		else if (ret == ERROR_NO_MORE_ITEMS) {
			task++;
			firstTime = 1;
		}
		else if (ret != ERROR_SUCCESS)
			break;
	}

	ProcessingDone();
}
