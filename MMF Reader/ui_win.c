#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <windows.h>
#include <shlobj.h>
#include <objbase.h>

//#pragma comment(lib,"ole32.lib")
//#pragma comment(lib,"Comdlg32.lib")
static int CALLBACK BrowseFolderCallback (HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
	if (uMsg == BFFM_INITIALIZED) {
		const char *path = (const char *) lpData;
		//printf ("Ruta: %s\n", path);
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM) path);
	}
	return 0;
}

char *ui_get_mmf_directory (void) {
	TCHAR path[MAX_PATH];
	BROWSEINFO binf = { 0 };

	binf.lpszTitle = ("Browse for folder...");
	binf.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	binf.lParam = NULL; /* Este parámetro es la ruta inicial */
	binf.lpfn = BrowseFolderCallback;

	LPITEMIDLIST pidl = SHBrowseForFolder (&binf);

	if (pidl != 0) {
		//get the name of the folder and put it in path
		SHGetPathFromIDList (pidl, path);

		//free memory used
		/*IMalloc * imalloc = 0;
		if (SUCCEEDED(SHGetMalloc (&imalloc))){
			imalloc->Free (pidl);
			imalloc->Release ();
		}*/

		return strdup (path);
	}

	return NULL;
}

void ui_init (int *argc, char ***argv) {
	CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

	OleInitialize (NULL);
}

void ui_show_message_error (const char *message) {
	MessageBox (NULL, (LPCTSTR) message, (LPCTSTR) "BKV Reader", MB_OK | MB_ICONERROR);
}

void ui_show_message_warning (const char *message) {
	MessageBox (NULL, (LPCTSTR) message, (LPCTSTR) "BKV Reader", MB_OK | MB_ICONWARNING);
}

void ui_show_message_info (const char *message) {
	MessageBox (NULL, (LPCTSTR) message, (LPCTSTR) "BKV Reader", MB_OK | MB_ICONINFORMATION);
}

char *ui_save_file (void) {
	OPENFILENAMEA ofn;

	WCHAR szFileName[MAX_PATH] = L"";

	memset (&ofn, 0, sizeof (ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = (LPCWSTR)"Wavefront OBJ file (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = (LPCWSTR)"obj";

	GetSaveFileName (&ofn);

	return strdup (szFileName);
}
