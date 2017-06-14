/******************************************************************************\
\******************************************************************************/

/* --- INCLUDES ------------------------------------------------------------- */
#include "..\..\..\AceFilterCore\PluginCommon.h"


/* --- PLUGIN DECLARATIONS -------------------------------------------------- */
#define PLUGIN_NAME         _T("MasterMailboxRelation")
#define PLUGIN_KEYWORD      _T("MMR")
#define PLUGIN_DESCRIPTION  _T("Outputs ACE in the format master->mailaddress->relation");

// Plugin informations
PLUGIN_DECLARE_NAME;
PLUGIN_DECLARE_KEYWORD;
PLUGIN_DECLARE_DESCRIPTION;

// Plugin functions
PLUGIN_DECLARE_INITIALIZE;
PLUGIN_DECLARE_FINALIZE;
PLUGIN_DECLARE_HELP;
PLUGIN_DECLARE_WRITEACE;

// Plugin requirements
PLUGIN_DECLARE_REQUIREMENT(PLUGIN_REQUIRE_SID_RESOLUTION);


/* --- DEFINES -------------------------------------------------------------- */
#define DEFAULT_MMR_OUTFILE                 _T("mmrout.csv")
#define DEFAULT_MMR_NO_RELATION_KEYWORD     _T("FILTERED_GENERIC_RELATION")
#define MMR_OUTFILE_HEADER                  {_T("dnMaster:START_ID"),_T("dnSlave:END_ID"),_T("keyword:TYPE")}
#define MMR_OUTFILE_HEADER_COUNT            (3)
#define MASTERMAILBOX_HEAP_NAME               _T("MMRHEAP")


/* --- PRIVATE VARIABLES ---------------------------------------------------- */
static PUTILS_HEAP gs_hHeapMasterSlave = INVALID_HANDLE_VALUE;
static LPTSTR gs_outfileName = NULL;
static LPTSTR gs_outDenyfileName = NULL;
static CSV_HANDLE gs_hOutfile;
static CSV_HANDLE gs_hOutDenyfile;

/* --- PUBLIC VARIABLES ----------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */
static void WriteRelation(
	_In_ PLUGIN_API_TABLE const * const api,
	_In_ LPTSTR master,
	_In_ LPTSTR slave,
	_In_ LPTSTR relationKeyword,
	_In_ CSV_HANDLE outFile
) {
	LPTSTR csvRecord[3];
	BOOL bResult = FALSE;
	DWORD csvRecordNumber = 3;
	csvRecord[0] = master;
	csvRecord[1] = slave;
	csvRecord[2] = relationKeyword;

	bResult = api->InputCsv.CsvWriteNextRecord(outFile, csvRecord, &csvRecordNumber);
	if (!bResult)
		API_FATAL(_T("Failed to write CSV outfile: <err:%#08x>"), api->InputCsv.CsvGetLastError(outFile));


}

/* --- PUBLIC FUNCTIONS ----------------------------------------------------- */
void PLUGIN_GENERIC_HELP(
	_In_ PLUGIN_API_TABLE const * const api
) {
	API_LOG(Bypass, _T("Specify the outfile with the <mmrout> plugin option"));
	API_LOG(Bypass, _T("The default outfile is <%s>"), DEFAULT_MMR_OUTFILE);
	API_LOG(Bypass, _T("Some filters do not associate 'relations' (and the corresponding keywords) to the ACE they filter,"));
	API_LOG(Bypass, _T("in this case, the generic relation keyword is used in the mmr outfile : <%s>"), DEFAULT_MMR_NO_RELATION_KEYWORD);
}

BOOL PLUGIN_GENERIC_INITIALIZE(
	_In_ PLUGIN_API_TABLE const * const api
) {
	BOOL bResult = FALSE;
	PTCHAR pptAttrsListForCsv[MMR_OUTFILE_HEADER_COUNT] = MMR_OUTFILE_HEADER;

	bResult = ApiHeapCreateX(&gs_hHeapMasterSlave, MASTERMAILBOX_HEAP_NAME, NULL);
	if (API_FAILED(bResult)) {
		return ERROR_VALUE;
	}

	gs_outfileName = api->Common.GetPluginOption(_T("mmrout"), FALSE);
	if (!gs_outfileName) {
		gs_outfileName = DEFAULT_MMR_OUTFILE;
	}

	gs_outDenyfileName = ApiHeapAllocX(gs_hHeapMasterSlave, (DWORD)((_tcslen(gs_outfileName) + 10) * sizeof(TCHAR)));
	_stprintf_s(gs_outDenyfileName, _tcslen(gs_outfileName) + 10, _T("%s.deny.csv"), gs_outfileName);
	API_LOG(Info, _T("Outfiles are <%s>/<%s>"), gs_outfileName, gs_outDenyfileName);

	bResult = api->InputCsv.CsvOpenWrite(gs_outfileName, MMR_OUTFILE_HEADER_COUNT, pptAttrsListForCsv, &gs_hOutfile);
	if (!bResult) {
		API_FATAL(_T("Failed to open CSV outfile <%s>: <err:%#08x>"), gs_outfileName, api->InputCsv.CsvGetLastError(gs_hOutfile));
	}
	bResult = api->InputCsv.CsvOpenWrite(gs_outDenyfileName, MMR_OUTFILE_HEADER_COUNT, pptAttrsListForCsv, &gs_hOutDenyfile);
	if (!bResult) {
		API_FATAL(_T("Failed to open CSV outfile <%s>: <err:%#08x>"), gs_outDenyfileName, api->InputCsv.CsvGetLastError(gs_hOutDenyfile));
	}
	return TRUE;
}

BOOL PLUGIN_GENERIC_FINALIZE(
	_In_ PLUGIN_API_TABLE const * const api
) {
	BOOL bResult = FALSE;

	if (!api->InputCsv.CsvClose(&gs_hOutfile) || !api->InputCsv.CsvClose(&gs_hOutDenyfile)) {
		API_LOG(Err, _T("Failed to close outfiles handles : <%u>"), GetLastError());
		return FALSE;
	}
	bResult = ApiHeapDestroyX(&gs_hHeapMasterSlave);
	if (API_FAILED(bResult)) {
		return ERROR_VALUE;
	}
	return TRUE;
}

BOOL PLUGIN_WRITER_WRITEACE(
	_In_ PLUGIN_API_TABLE const * const api,
	_Inout_ PIMPORTED_ACE ace
) {
	DWORD i = 0;
	DWORD relCount = 0;
	LPTSTR resolvedTrustee = NULL;
	LPTSTR resolvedMail = NULL;

	resolvedTrustee = api->Resolver.ResolverGetAceTrusteeStr(ace);
	resolvedMail = api->Resolver.ResolverGetAceObjectMail(ace);
	if (!resolvedMail) {
		API_LOG(Dbg, _T("Object has mbx sd without mail address : <%s>"), ace->imported.objectDn);
		return TRUE;
	}

	for (i = 0; i < ACE_REL_COUNT; i++) {
		if (HAS_RELATION(ace, i)) {
			relCount++;
			if (IS_ALLOWED_ACE(ace->imported.raw))
				WriteRelation(api, resolvedTrustee, resolvedMail, api->Ace.GetAceRelationStr(i), gs_hOutfile);
			else
				WriteRelation(api, resolvedTrustee, resolvedMail, api->Ace.GetAceRelationStr(i), gs_hOutDenyfile);
		}
	}
	return TRUE;
}
