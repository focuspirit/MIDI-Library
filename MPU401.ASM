;
;	Roland MPU-401 Processing Unit I/O Function
;
;		Data Port	      : 330H	(Read/Write)
;		Command / Status Port : 331H	(Write)
;
;			Code : S.G. Chang
;

.286
.MODEL	LARGE, C
PUBLIC	SendMPU, CtrlMPU, ReadMPU, MPUport

.DATA
MPUport DW	330H
.CODE
;///////////////////////////////////////////////////
SendMPU PROC	FAR

	mov	dx,MPUport

	inc	dx			;dx=331
	mov	ah,al			;
	mov	bx,1024 		;try
begin:
	mov	CX,2000 		;Try Count
Chance:
	in	AL,DX
	test	AL,040h 		;Data Set Ready
	jne	ReadIn

	dec	dx			;DX=Data Port
	mov	al,ah
	out	dx,al			;Out Data
	mov	cx,10
Delay:
	in	al,dx
	loop	Delay

	xor	ax,ax			;O.K.
	ret
ReadIn:
	test	al,80h			;Data Read Ready
	jne	ReTry

	dec	dx			;dx=330
	in	al,dx
	inc	dx			;dx=331
ReTry:
	loop	Chance
	dec	bx
	jnz	begin
	mov	ax,0ffffh		;Error
exit:
	ret
SendMPU ENDP
;///////////////////////////////////////////////////

CtrlMPU PROC	FAR

	mov	dx,MPUport

	inc	dx			;dx=331
	mov	ah,al			;Save Data
	xor	cx,cx
Check:
	in	al,dx
	test	al,40h			;Data Set Ready
	jz	Send
	loop	Check
	mov	ax,0ffffh		;Error
	ret
Send:
	mov	al,ah
	out	dx,al			;Send 331 command
	xor	cx,cx
Empty:
	in	al,dx
	test	al,80h			;Data Read Ready
	jnz	NextLoop

	dec	dx			;dx=330
	in	al,dx			;Read In form 330
	cmp	al,0feh 		;ACK
	jz	exit			;O.K.
	inc	dx
NextLoop:
	loop	Empty
exit:
	xor	ax,ax
	ret
CtrlMPU ENDP

ReadMPU PROC	FAR

	mov	dx,MPUport
	inc	dx			;dx=331

	xor	cx,cx
	mov	ax,cx
Busy:
	in	al,dx
	test	al,80h			;Data Read Ready
	jz	Read
	loop	Busy
	mov	ax,0ffffh
	ret
Read:
	dec	dx			;dx=330
	in	al,dx
	ret
ReadMPU ENDP

	END
