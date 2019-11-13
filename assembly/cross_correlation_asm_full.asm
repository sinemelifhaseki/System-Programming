;Sinem Elif Haseki 150160026 - Assignment 1
segment .bss
counter1 resd 1
counter2 resd 1
f1 resd 1
f2 resd 1
result resd 1

segment .text
global cross_correlation_asm_full

cross_correlation_asm_full:
	push ebp
    mov ebp,esp
	push ebx
	sub esp, 4

    mov ecx, [ebp+28] ;output size is in ecx
    mov ebx, [ebp+24] ;ebx now holds the first address of output array
    
zeros: ;for making the initial values of output array 0
	mov dword [ebx], 0
	dec ecx
	jz initialize
	add ebx, 4
	jmp zeros
	
initialize:
	mov ecx, [ebp+12] ;size1 to c1
	mov edx, [ebp+20] ;size2 to c2
	mov dword [f1], 0
	mov dword [f2], 0
	mov dword [result], 0
	mov dword [counter1], ecx
	mov dword [counter2], edx
	
cross:	
	mov dword eax, 4
	mov edx, [f1]
	mul edx
	mov ecx, [ebp+8] ;array 1
	add ecx, eax
	mov ecx, [ecx]
inner:
	mov ebx, [f2]
	mov dword eax, 4
	mul ebx
	mov edx, [ebp+16] ;array 2	
	add edx, eax
	mov eax, [edx]
	mul ecx ;a1[i]*a2[i] -> eax
	mov [ebp-8], eax ;result is stored in the local var     
;calculate the index for output to be stored
	mov edx, [ebp+20] ;size2 in edx
	dec edx ;size2 - 1
	mov ebx, [f1]
	mov eax, [f2]
	add edx, ebx
	sub edx, eax ;output index is at edx, we have to multiply it by 4 to add it to starting address of output
	mov dword eax, 4
	mul edx ;how much to be added is in eax
	mov ebx, [ebp+24]
	add ebx, eax ;final address of output element is in ebx
	mov edx,[ebp-8] ;result calculated is fetched to edx
	add edx, [ebx] ;prev value is added to edx
	mov [ebx], edx
	dec dword [counter2]
	jz outer
	inc dword [f2]
	jmp inner
outer:
	mov edx, [ebp+20] ;size2 into edx
	mov [counter2], edx ;counter2 is back to being size2
	dec dword [counter1]
	jz finish
	mov dword [f2], 0
	inc dword [f1]
	jmp cross

finish:
	mov ecx, [ebp+24] ;outputs first address to ecx
	mov edx, [ebp+28] ;output size to edx
continue:
	cmp dword [ecx], 0
	jne nonzero
	add dword ecx, 4
	dec edx
	jz endofprogram
	jmp continue
	
nonzero:
	inc dword [result]
	add ecx, 4
	dec edx
	jz endofprogram
	jmp continue	

endofprogram:    
	mov eax, [result]
    add esp,4
    pop ebx
    mov esp, ebp ;sub esp,4 
    pop ebp
    ret
