
	ORG		100h
	USE16
	CPU		8086
	
%define	VERSION_ID		"20251103"

START:			jmp		short setup
hlthandler:		sti
				hlt
				cli
				jmp 	far [cs:org_int28]
				
org_int28		dd		0

setup:			cld 		; カウンタインクリメント指示
				push	cs ; CSをスタックに
				pop		ds ; スタックからDSにセット(CS -> DS)
				mov		ah, 9 ; $ 終端文字列表示
				mov		dx, title_msg ; タイトル
				int		21h ; 表示実行
				
				; ES <- 0
				xor ax, ax
				mov es, ax
				
				; 元のINT 28hを退避
				mov ax, word [es:00a0h] ; AX <- old_offset (ES:0a0h)
				mov word [org_int28 + 0], ax ; org_int28 offset <- old_offset
				mov ax, word [es:00a2h] ; AX <- old_segment (ES:0a2h)
				mov word [org_int28 + 2], ax ; org_int28 segment <- old_segment
				
				; INT 28hハンドラを登録
				mov word [es:00a0h], hlthandler ; write new offset
				mov word [es:00a2h], cs ; write new segment
				
				; 常駐しておしまい
				mov		ax, 3100h
				mov		dx, setup + 15
				mov		cl, 4
				shr		dx, cl ; bytes -> paragraphs
				int		21h

title_msg		db	"HLT idle driver for np2+dos ("
				db	VERSION_ID
				db	 ")"
				db	13, 10, "$"

