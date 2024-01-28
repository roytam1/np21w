/**
 * @file	d_hostdrv.cpp
 * @brief	HOSTDRV �ݒ�_�C�A���O
 */

#include "compiler.h"
#include "resource.h"
#include "dialog.h"
#include "c_combodata.h"
#include "np2.h"
#include "commng.h"
#include "sysmng.h"
#include "misc/DlgProc.h"
#include "pccore.h"
#include "common/strres.h"
#include "hostdrv.h"

#include <shlobj.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if(uMsg==BFFM_INITIALIZED){
        SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM)TRUE, lpData);
    }
    return 0;
}

/**
 * @brief IDE �ݒ�_�C�A���O
 * @param[in] hwndParent �e�E�B���h�E
 */
class CHostdrvDlg : public CDlgProc
{
public:
	CHostdrvDlg(HWND hwndParent);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual LRESULT WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam);

private:
	TCHAR m_hdrvenable;			//!< �L��
	TCHAR m_hdrvroot[MAX_PATH];	//!< ���L�f�B���N�g��
	UINT8 m_hdrvacc;			//!< �A�N�Z�X����
	CWndProc m_chkenabled;		//!< Enabled
	CWndProc m_edtdir;			//!< Shared Directory
	CWndProc m_chkread;			//!< Permission: Read
	CWndProc m_chkwrite;		//!< Permission: Write
	CWndProc m_chkdelete;		//!< Permission: Delete
};

/**
 * �R���X�g���N�^
 * @param[in] hwndParent �e�E�B���h�E
 */
CHostdrvDlg::CHostdrvDlg(HWND hwndParent)
	: CDlgProc(IDD_HOSTDRV, hwndParent)
{
}

/**
 * ���̃��\�b�h�� WM_INITDIALOG �̃��b�Z�[�W�ɉ������ČĂяo����܂�
 * @retval TRUE �ŏ��̃R���g���[���ɓ��̓t�H�[�J�X��ݒ�
 * @retval FALSE ���ɐݒ��
 */
BOOL CHostdrvDlg::OnInitDialog()
{
	
	_tcscpy(m_hdrvroot, np2cfg.hdrvroot);
	m_hdrvacc = np2cfg.hdrvacc;
	m_hdrvenable= np2cfg.hdrvenable;
	
	m_chkenabled.SubclassDlgItem(IDC_HOSTDRVENABLE, this);
	if(m_hdrvenable)
		m_chkenabled.SendMessage(BM_SETCHECK , BST_CHECKED , 0);
	else
		m_chkenabled.SendMessage(BM_SETCHECK , BST_UNCHECKED , 0);
	
	m_edtdir.SubclassDlgItem(IDC_HOSTDRVDIR, this);
	m_edtdir.SetWindowText(m_hdrvroot);
	
	m_chkread.SubclassDlgItem(IDC_HOSTDRVREAD, this);
	if(m_hdrvacc & HDFMODE_READ)
		m_chkread.SendMessage(BM_SETCHECK , BST_CHECKED , 0);
	else
		m_chkread.SendMessage(BM_SETCHECK , BST_UNCHECKED , 0);
	
	m_chkwrite.SubclassDlgItem(IDC_HOSTDRVWRITE, this);
	if(m_hdrvacc & HDFMODE_WRITE)
		m_chkwrite.SendMessage(BM_SETCHECK , BST_CHECKED , 0);
	else
		m_chkwrite.SendMessage(BM_SETCHECK , BST_UNCHECKED , 0);

	m_chkdelete.SubclassDlgItem(IDC_HOSTDRVDELETE, this);
	if(m_hdrvacc & HDFMODE_DELETE)
		m_chkdelete.SendMessage(BM_SETCHECK , BST_CHECKED , 0);
	else
		m_chkdelete.SendMessage(BM_SETCHECK , BST_UNCHECKED , 0);

	m_edtdir.SetFocus();

	return FALSE;
}

/**
 * ���[�U�[�� OK �̃{�^�� (IDOK ID ���̃{�^��) ���N���b�N����ƌĂяo����܂�
 */
void CHostdrvDlg::OnOK()
{
	UINT update = 0;
	UINT32 valtmp;
	TCHAR numbuf[31];
	
	if (m_hdrvenable!=np2cfg.hdrvenable || _tcscmp(np2cfg.hdrvroot, m_hdrvroot)!=0 || m_hdrvacc!=np2cfg.hdrvacc)
	{
		np2cfg.hdrvenable = m_hdrvenable;
		_tcscpy(np2cfg.hdrvroot, m_hdrvroot);
		np2cfg.hdrvacc = m_hdrvacc;
		update |= SYS_UPDATECFG;
	}

	sysmng_update(update);

	CDlgProc::OnOK();
}

/**
 * ���[�U�[�����j���[�̍��ڂ�I�������Ƃ��ɁA�t���[�����[�N�ɂ���ČĂяo����܂�
 * @param[in] wParam �p�����^
 * @param[in] lParam �p�����^
 * @retval TRUE �A�v���P�[�V���������̃��b�Z�[�W����������
 */
BOOL CHostdrvDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	OEMCHAR hdrvroottmp[MAX_PATH];
	int hdrvpathlen;
	switch (LOWORD(wParam))
	{
		case IDC_HOSTDRVENABLE:
			m_hdrvenable = m_chkenabled.SendMessage(BM_GETCHECK , 0 , 0);
			return TRUE;

		case IDC_HOSTDRVDIR:
			m_edtdir.GetWindowText(hdrvroottmp, NELEMENTS(hdrvroottmp));
			hdrvpathlen = _tcslen(hdrvroottmp);
			if(hdrvroottmp[hdrvpathlen-1]=='\\'){
				hdrvroottmp[hdrvpathlen-1] = '\0';
			}
			if(_tcscmp(hdrvroottmp, m_hdrvroot)!=0){
				_tcscpy(m_hdrvroot, hdrvroottmp);
				m_hdrvacc = (m_hdrvacc & ~(HDFMODE_WRITE|HDFMODE_DELETE));
				m_chkwrite.SendMessage(BM_SETCHECK , BST_UNCHECKED , 0);
				m_chkdelete.SendMessage(BM_SETCHECK , BST_UNCHECKED , 0);
			}
			return TRUE;

		case IDC_HOSTDRVBROWSE:
			{
				OEMCHAR name[MAX_PATH],dir[MAX_PATH];
				BROWSEINFO  binfo;
				LPITEMIDLIST idlist;
    
				m_edtdir.GetWindowText(hdrvroottmp, NELEMENTS(hdrvroottmp));

				binfo.hwndOwner = g_hWndMain;
				binfo.pidlRoot = NULL;
				binfo.pszDisplayName = name;
				binfo.lpszTitle = OEMTEXT("");
				binfo.ulFlags = BIF_RETURNONLYFSDIRS; 
				binfo.lpfn = BrowseCallbackProc;              
				binfo.lParam = (LPARAM)(hdrvroottmp);               
				binfo.iImage = 0;
    
				if((idlist = SHBrowseForFolder(&binfo))){
					SHGetPathFromIDList(idlist, dir);
					_tcscpy(hdrvroottmp, dir);
					hdrvpathlen = _tcslen(hdrvroottmp);
					if(hdrvroottmp[hdrvpathlen-1]=='\\'){
						hdrvroottmp[hdrvpathlen-1] = '\0';
					}
					m_edtdir.SetWindowText(hdrvroottmp);
					CoTaskMemFree(idlist);               
				}
			}
			return TRUE;

		case IDC_HOSTDRVREAD:
			m_hdrvacc = (m_hdrvacc & ~HDFMODE_READ);
			m_hdrvacc |= (m_chkread.SendMessage(BM_GETCHECK , 0 , 0) ? HDFMODE_READ : 0);
			return TRUE;

		case IDC_HOSTDRVWRITE:
			m_hdrvacc = (m_hdrvacc & ~HDFMODE_WRITE);
			m_hdrvacc |= (m_chkwrite.SendMessage(BM_GETCHECK , 0 , 0) ? HDFMODE_WRITE : 0);
			return TRUE;

		case IDC_HOSTDRVDELETE:
			m_hdrvacc = (m_hdrvacc & ~HDFMODE_DELETE);
			m_hdrvacc |= (m_chkdelete.SendMessage(BM_GETCHECK , 0 , 0) ? HDFMODE_DELETE : 0);
			return TRUE;
	}
	return FALSE;
}

/**
 * CWndProc �I�u�W�F�N�g�� Windows �v���V�[�W�� (WindowProc) ���p�ӂ���Ă��܂�
 * @param[in] nMsg ��������� Windows ���b�Z�[�W���w�肵�܂�
 * @param[in] wParam ���b�Z�[�W�̏����Ŏg���t������񋟂��܂��B���̃p�����[�^�̒l�̓��b�Z�[�W�Ɉˑ����܂�
 * @param[in] lParam ���b�Z�[�W�̏����Ŏg���t������񋟂��܂��B���̃p�����[�^�̒l�̓��b�Z�[�W�Ɉˑ����܂�
 * @return ���b�Z�[�W�Ɉˑ�����l��Ԃ��܂�
 */
LRESULT CHostdrvDlg::WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return CDlgProc::WindowProc(nMsg, wParam, lParam);
}

/**
 * �R���t�B�O �_�C�A���O
 * @param[in] hwndParent �e�E�B���h�E
 */
void dialog_hostdrvopt(HWND hwndParent)
{
	CHostdrvDlg dlg(hwndParent);
	dlg.DoModal();
}
