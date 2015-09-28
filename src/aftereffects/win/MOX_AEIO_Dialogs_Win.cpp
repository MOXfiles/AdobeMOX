

#include "MOX_AEIO_Dialogs.h"

#include <Windows.h>
#include <commctrl.h>

#include <stdio.h>


enum {
	OUT_noUI = -1,
	OUT_OK = IDOK,
	OUT_Cancel = IDCANCEL,
	OUT_Lossless_Check,
	OUT_Quality_Label,
	OUT_Quality_Slider,
	OUT_Quality_Readout,
	OUT_BitDepth_Menu,
	OUT_Codec_Menu
};

// sensible Win macros
#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

#define ENABLE_ITEM(ITEM, ENABLE)	EnableWindow(GetDlgItem(hwndDlg, (ITEM)), (ENABLE));

#define ADD_MENU_ITEM(MENU, INDEX, STRING, VALUE, SELECTED) \
				SendMessage(GET_ITEM(MENU),( UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)STRING ); \
				SendMessage(GET_ITEM(MENU),(UINT)CB_SETITEMDATA, (WPARAM)INDEX, (LPARAM)(DWORD)VALUE); \
				if(SELECTED) \
					SendMessage(GET_ITEM(MENU), CB_SETCURSEL, (WPARAM)INDEX, (LPARAM)0);

#define GET_MENU_VALUE(MENU)		SendMessage(GET_ITEM(MENU), (UINT)CB_GETITEMDATA, (WPARAM)SendMessage(GET_ITEM(MENU),(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0), (LPARAM)0)



static bool					g_lossless = TRUE;
static int					g_quality = 80;
static DialogBitDepth		g_bit_depth = DIALOG_BITDEPTH_AUTO;
static DialogCodec			g_codec = DIALOG_CODEC_AUTO;


static WORD	g_item_clicked = 0;


static void TrackLossless(HWND hwndDlg)
{
	const bool lossy = !GET_CHECK(OUT_Lossless_Check);

	ENABLE_ITEM(OUT_Quality_Label, lossy);
	ENABLE_ITEM(OUT_Quality_Slider, lossy);
	ENABLE_ITEM(OUT_Quality_Readout, lossy);
}


static void TrackSlider(HWND hwndDlg)
{
	int val = SendMessage(GET_ITEM(OUT_Quality_Slider), TBM_GETPOS, (WPARAM)0, (LPARAM)0 );

	char txt[5];
	sprintf_s(txt, 4, "%d", val);

	SetDlgItemText(hwndDlg, OUT_Quality_Readout, txt);
}


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
			SET_CHECK(OUT_Lossless_Check, g_lossless);

			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETRANGEMIN, (WPARAM)(BOOL)FALSE, (LPARAM)1);
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETRANGEMAX, (WPARAM)(BOOL)FALSE, (LPARAM)100);
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)g_quality);

			ADD_MENU_ITEM(OUT_BitDepth_Menu, 0, "Auto", DIALOG_BITDEPTH_AUTO, (g_bit_depth == DIALOG_BITDEPTH_AUTO));
			ADD_MENU_ITEM(OUT_BitDepth_Menu, 1, "8-bit", DIALOG_BITDEPTH_8, (g_bit_depth == DIALOG_BITDEPTH_8));
			ADD_MENU_ITEM(OUT_BitDepth_Menu, 2, "16-bit", DIALOG_BITDEPTH_16, (g_bit_depth == DIALOG_BITDEPTH_16));
			ADD_MENU_ITEM(OUT_BitDepth_Menu, 3, "16-bit float", DIALOG_BITDEPTH_16_FLOAT, (g_bit_depth == DIALOG_BITDEPTH_16_FLOAT));
			ADD_MENU_ITEM(OUT_BitDepth_Menu, 4, "32-bit float", DIALOG_BITDEPTH_32_FLOAT, (g_bit_depth == DIALOG_BITDEPTH_32_FLOAT));

			ADD_MENU_ITEM(OUT_Codec_Menu, 0, "Auto", DIALOG_CODEC_AUTO, (g_codec == DIALOG_CODEC_AUTO));
			ADD_MENU_ITEM(OUT_Codec_Menu, 1, "Dirac", DIALOG_CODEC_DIRAC, (g_codec == DIALOG_CODEC_DIRAC));
			ADD_MENU_ITEM(OUT_Codec_Menu, 2, "OpenEXR", DIALOG_CODEC_OPENEXR, (g_codec == DIALOG_CODEC_OPENEXR));
			ADD_MENU_ITEM(OUT_Codec_Menu, 3, "PNG", DIALOG_CODEC_PNG, (g_codec == DIALOG_CODEC_PNG));
			ADD_MENU_ITEM(OUT_Codec_Menu, 4, "Uncompressed", DIALOG_CODEC_UNCOMPRESSED, (g_codec == DIALOG_CODEC_UNCOMPRESSED));

			TrackSlider(hwndDlg);
			TrackLossless(hwndDlg);

			return TRUE;
 
		case WM_NOTIFY:
			switch(LOWORD(wParam))
			{
				case OUT_Quality_Slider:
					TrackSlider(hwndDlg);
				return TRUE;
			}
		return FALSE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch(g_item_clicked)
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different
					g_lossless = GET_CHECK(OUT_Lossless_Check);
					g_quality = SendMessage(GET_ITEM(OUT_Quality_Slider), TBM_GETPOS, (WPARAM)0, (LPARAM)0 );
					g_bit_depth = (DialogBitDepth)GET_MENU_VALUE(OUT_BitDepth_Menu);
					g_codec = (DialogCodec)GET_MENU_VALUE(OUT_Codec_Menu);

					EndDialog(hwndDlg, 0);
					return TRUE;

				case OUT_Lossless_Check:
					TrackLossless(hwndDlg);
					return TRUE;
            } 
    } 
    return FALSE; 
} 

static HINSTANCE hDllInstance = NULL;

bool
MOX_AEIO_Video_Out_Dialog(
	DialogBitDepth	&bitDepth,
	bool			&lossless,
	int				&quality, // 1-100
	DialogCodec		&codec,
	const void		*plugHndl,
	const void		*mwnd)
{
	g_lossless		= lossless;
	g_quality		= quality;
	g_bit_depth		= bitDepth;
	g_codec			= codec;


	int status = DialogBox(hDllInstance, (LPSTR)"OUT_DIALOG", (HWND)mwnd, (DLGPROC)DialogProc);


	if(g_item_clicked == OUT_OK)
	{
		lossless		= g_lossless;
		quality			= g_quality;
		bitDepth		= g_bit_depth;
		codec			= g_codec;

		return true;
	}
	else
		return false;
}


BOOL WINAPI DllMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		hDllInstance = (HINSTANCE)hInstance;

	return TRUE;   // Indicate that the DLL was initialized successfully.
}
