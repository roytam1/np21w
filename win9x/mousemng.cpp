#include	"compiler.h"
#include	"np2.h"
#include	"mousemng.h"

#define DIRECTINPUT_VERSION 0x0800
#include	<dinput.h>
#pragma comment(lib, "dinput8.lib")

#define	MOUSEMNG_RANGE		128


typedef struct {
	SINT16	x;
	SINT16	y;
	UINT8	btn;
	UINT	flag;
} MOUSEMNG;

static	MOUSEMNG	mousemng;
static  int mousecaptureflg = 0;

static  SINT16 mouseMul = 1; // �}�E�X�X�s�[�h�{���i���q�j
static  SINT16 mouseDiv = 1; // �}�E�X�X�s�[�h�{���i����j

static  SINT16 mousebufX = 0; // �}�E�X�ړ��o�b�t�@(X)
static  SINT16 mousebufY = 0; // �}�E�X�ړ��o�b�t�@(Y)

// RAW�}�E�X���͑Ή� np21w ver0.86 rev13
static  LPDIRECTINPUT8 dinput = NULL; 
static  LPDIRECTINPUTDEVICE8 diRawMouse = NULL; 
static  int mouseRawDeltaX = 0;
static  int mouseRawDeltaY = 0;

UINT8 mousemng_getstat(SINT16 *x, SINT16 *y, int clear) {
	*x = mousemng.x;
	*y = mousemng.y;
	if (clear) {
		mousemng.x = 0;
		mousemng.y = 0;
	}
	return(mousemng.btn);
}

UINT8 mousemng_supportrawinput() {
	return(dinput && diRawMouse);
}

// ----

static void getmaincenter(POINT *cp) {

	RECT	rct;

	GetWindowRect(g_hWndMain, &rct);
//#ifdef SUPPORT_WAB
//	// XXX: �𑜓x��ς���ƒ��S���W���ς���Ă��܂��̂ő΍�i�蔲���j
//	rct.right = rct.left + 640;
//	rct.bottom = rct.top + 400;
//#endif
	cp->x = (rct.right + rct.left) / 2;
	cp->y = (rct.bottom + rct.top) / 2;
}

static void initDirectInput(){
	
	HRESULT		hr;

	if(!dinput){
		//hr = DirectInputCreateEx(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput7, (void**)&dinput, NULL);
		hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&dinput, NULL); // �֐����ς��Ă₪����( ߄t�)
		if (!FAILED(hr)){
			hr = dinput->CreateDevice(GUID_SysMouse, &diRawMouse, NULL);
			if (!FAILED(hr)){
				// �f�[�^�t�H�[�}�b�g�ݒ�
				hr = diRawMouse->SetDataFormat(&c_dfDIMouse);
				if (!FAILED(hr)){
					// �������x���ݒ�
					hr = diRawMouse->SetCooperativeLevel(g_hWndMain, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
				}
				if (!FAILED(hr)){
					// �f�o�C�X�ݒ�
					DIPROPDWORD		diprop;
					diprop.diph.dwSize = sizeof(diprop);
					diprop.diph.dwHeaderSize = sizeof(diprop.diph);
					diprop.diph.dwObj = 0;
					diprop.diph.dwHow = DIPH_DEVICE;
					diprop.dwData = DIPROPAXISMODE_REL;	// ���Βl���[�h
					hr = diRawMouse->SetProperty(DIPROP_AXISMODE, &diprop.diph);
				}
				if (!FAILED(hr)) {
					// ���͊J�n
					hr = diRawMouse->Acquire();
					if (!FAILED(hr)) {
						// ���s���
						diRawMouse->Release();
						diRawMouse = NULL;
						dinput->Release();
						dinput = NULL;
					}
				}else{
					// ���s���
					diRawMouse->Release();
					diRawMouse = NULL;
					dinput->Release();
					dinput = NULL;
				}
			}else{
				// ���s���
				dinput->Release();
				dinput = NULL;
			}
		}
	}
}
static void destroyDirectInput(){
	if(diRawMouse){
		diRawMouse->Release();
		diRawMouse = NULL;
	}
	if(dinput){
		dinput->Release();
		dinput = NULL;
	}
}

static void mousecapture(BOOL capture) {

	LONG	style;
	POINT	cp;
	RECT	rct;

	mouseMul = np2oscfg.mousemul != 0 ? np2oscfg.mousemul : 1;
	mouseDiv = np2oscfg.mousediv != 0 ? np2oscfg.mousediv : 1;

	style = GetClassLong(g_hWndMain, GCL_STYLE);
	if (capture) {
		ShowCursor(FALSE);
		getmaincenter(&cp);
		rct.left = cp.x - MOUSEMNG_RANGE;
		rct.right = cp.x + MOUSEMNG_RANGE;
		rct.top = cp.y - MOUSEMNG_RANGE;
		rct.bottom = cp.y + MOUSEMNG_RANGE;
		SetCursorPos(cp.x, cp.y);
		ClipCursor(&rct);
		style &= ~(CS_DBLCLKS);
		mousecaptureflg = 1;
		if(np2oscfg.rawmouse){
			initDirectInput();
		}
	}
	else {
		ShowCursor(TRUE);
		ClipCursor(NULL);
		style |= CS_DBLCLKS;
		mousecaptureflg = 0;
		if(np2oscfg.rawmouse){
			destroyDirectInput();
		}
	}
	SetClassLong(g_hWndMain, GCL_STYLE, style);
}

void mousemng_initialize(void) {

	ZeroMemory(&mousemng, sizeof(mousemng));
	mousemng.btn = uPD8255A_LEFTBIT | uPD8255A_RIGHTBIT;
	mousemng.flag = (1 << MOUSEPROC_SYSTEM);

}

void mousemng_destroy(void) {
	destroyDirectInput();
}

void mousemng_sync(void) {

	POINT	p;
	POINT	cp;

	if ((!mousemng.flag) && (GetCursorPos(&p))) {
		getmaincenter(&cp);
		if(np2oscfg.rawmouse && dinput==NULL)
			initDirectInput();
		if(np2oscfg.rawmouse && mousemng_supportrawinput()){
			DIMOUSESTATE diMouseState = {0};
			HRESULT hr;
			hr = diRawMouse->GetDeviceState(sizeof(DIMOUSESTATE), &diMouseState);
			if (hr != DI_OK){
				destroyDirectInput();
				initDirectInput();
			}else{
				mousebufX += (SINT16)(diMouseState.lX)*mouseMul;
				mousebufY += (SINT16)(diMouseState.lY)*mouseMul;
			}
		}else{
			mousebufX += (SINT16)(p.x - cp.x)*mouseMul;
			mousebufY += (SINT16)(p.y - cp.y)*mouseMul;
		}
		if(mousebufX >= mouseDiv || mousebufX <= -mouseDiv){
			mousemng.x += mousebufX / mouseDiv;
			mousebufX   = mousebufX % mouseDiv;
		}
		if(mousebufY >= mouseDiv || mousebufY <= -mouseDiv){
			mousemng.y += mousebufY / mouseDiv;
			mousebufY   = mousebufY % mouseDiv;
		}
		//mousemng.x += (SINT16)((p.x - cp.x));// / 2);
		//mousemng.y += (SINT16)((p.y - cp.y));// / 2);
		SetCursorPos(cp.x, cp.y);
	}
}

BOOL mousemng_buttonevent(UINT event) {

	if (!mousemng.flag) {
		switch(event) {
			case MOUSEMNG_LEFTDOWN:
				mousemng.btn &= ~(uPD8255A_LEFTBIT);
				break;

			case MOUSEMNG_LEFTUP:
				mousemng.btn |= uPD8255A_LEFTBIT;
				break;

			case MOUSEMNG_RIGHTDOWN:
				mousemng.btn &= ~(uPD8255A_RIGHTBIT);
				break;

			case MOUSEMNG_RIGHTUP:
				mousemng.btn |= uPD8255A_RIGHTBIT;
				break;
		}
		return(TRUE);
	}
	else {
		return(FALSE);
	}
}

void mousemng_enable(UINT proc) {

	UINT	bit;

	bit = 1 << proc;
	if (mousemng.flag & bit) {
		mousemng.flag &= ~bit;
		if (!mousemng.flag) {
			mousecapture(TRUE);
		}
	}
}

void mousemng_disable(UINT proc) {

	if (!mousemng.flag) {
		mousecapture(FALSE);
	}
	mousemng.flag |= (1 << proc);
}

void mousemng_toggle(UINT proc) {

	if (!mousemng.flag) {
		mousecapture(FALSE);
	}
	mousemng.flag ^= (1 << proc);
	if (!mousemng.flag) {
		mousecapture(TRUE);
	}
}

void mousemng_updateclip(){
	if(mousecaptureflg){
		mousecapture(FALSE);
		mousecapture(TRUE); // �L���v�`��������
	}
}