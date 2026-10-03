	.386
NEED	segment use16 at 0
zero	proc far
zero	endp
NEED	ends

cseg	segment use16
	org	100H
	assume	cs:cseg,ds:cseg
BEGIN	proc	near
	jmp INIT

setter:
	cmp	ah,2ah
	jne	dosjmp
	mov	cx,1993
	mov	dx,0a0ah
	iret
dosjmp:	jmp	zero

INIT:
	push cs
	pop ds

	mov ah,9
	mov dx,offset fakedate
	int 21h

	mov ax,3521h
	int 21h

	mov si,80h
	mov cl,byte ptr [si]
lp:
	dec cl
	jle load
	inc si
	cmp byte ptr [si],20h
	je lp
	cmp byte ptr [si],'/'
	jne load
	cmp byte ptr [si+1],'u'
	je ul
	cmp byte ptr [si+1],'U'
	jne load
ul:
	call instaled
	mov dx,offset unist
	jne sc
	push bx
	push es
	mov dx,word ptr es:[bx+dosjmp-setter+1]
	mov ds,word ptr es:[bx+dosjmp-setter+3]
	mov ax,2521h
	int 21h
	push cs
	pop ds
	mov dx,offset unist
	mov ah,9
	int 21h
	pop ax
	pop dx
	push cs
	pop cx
	sub dx,offset setter
	jnc f
	sub ax,1000h
f:
	sub cx,ax
	shl cx,4
	sub cx,dx
	cmp cx,offset INIT
	jne noul

	mov ds,ax
	int 27h


load:
	call instaled
	jne notinst
	mov ah,9
	mov dx,offset already
	int 21h
	mov dx,offset ulinst
sc:
	mov ah,9
	int 21h
noul:	int 20h

notinst:
	mov word ptr dosjmp+1,bx
	mov word ptr dosjmp+3,es
	mov dx,offset setter
	mov ax,2521h
	int 21h
	mov dx,offset inst
	mov ah,9
	int 21h
	mov dx,offset ulinst
	int 21h
	mov dx,offset INIT
	int     27h

instaled:
	mov	ax,word ptr es:[bx]
	cmp	ax,word ptr cs:[setter]
	jne	no
	mov	ax,word ptr es:[bx+2]
	cmp	ax,word ptr cs:[setter+2]
	jne	no
	mov	ax,word ptr es:[bx+4]
	cmp	ax,word ptr cs:[setter+4]
	jne	no
	mov	ax,word ptr es:[bx+6]
	cmp	ax,word ptr cs:[setter+6]
	jne	no
	mov	ax,word ptr es:[bx+8]
	cmp	ax,word ptr cs:[setter+8]
no:	ret

fakedate db 'Doom Press Release Version FakeDate by Lee Killough',0ah,0dh,'$'
already  db 'FakeDate is already installed.',0ah,0dh,'$'
ulinst   db 'Type FakeDate /u to uninstall.',0ah,0dh,'$'
inst     db 'FakeDate installed.',0ah,0dh,'You can now play the Doom Press Release Version.',0ah,0dh,'$'
unist    db 'FakeDate uninstalled.',0ah,0dh,'$'

BEGIN endp
cseg ends
end BEGIN
