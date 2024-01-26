#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <stdbool.h>

char* InputStream;
char
Input()
{
	char result = *InputStream;
	
	InputStream += (result != 0);

	return result;
}

void
Output(char c)
{
	putc(c, stdout);
}

int
main(int argc, char** argv)
{
	char* source_code         = 0;
	long int source_code_size = 0;
	InputStream               = (argc == 3 ? argv[2] : "");

	if (argc != 2 && argc != 3) fprintf(stderr, "Illegal number of arguments, expected: noufuck <source-file> [input]\n");
	else
	{
		FILE* source_file = 0;
		errno_t open_err = fopen_s(&source_file, argv[1], "rb");

		if (open_err != 0) fprintf(stderr, "Failed to open source file\n");
		else
		{
			fseek(source_file, 0, SEEK_END);
			source_code_size = ftell(source_file);
			rewind(source_file);

			char* buffer = malloc(source_code_size + 1);

			if (buffer == 0 || fread(buffer, 1, source_code_size, source_file) != source_code_size) fprintf(stderr, "Failed to read source file\n");
			else
			{
				buffer[source_code_size] = 0;
				source_code = buffer;
			}

			fclose(source_file);
		}
	}

	if (source_code != 0)
	{
		// TODO: prevent instruction pointer overrun
		void* inst = VirtualAlloc(0, 0xFFFF, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		void* data = VirtualAlloc(0, 0xFFFF, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		assert(inst != 0 && data != 0);

		char* inst_cursor = inst;
		char* bracket_stack[256]        = {0};
		unsigned int bracket_stack_size = 0;

		// NOTE: pologue
		// mov r12, rcx
		*inst_cursor++ = 0x49;
		*inst_cursor++ = 0x89;
		*inst_cursor++ = 0xCC;
		// sub rsp, 16
		*inst_cursor++ = 0x48;
		*inst_cursor++ = 0x83;
		*inst_cursor++ = 0xEC;
		*inst_cursor++ = 0x10;
		// mov rax, Output
		*inst_cursor++ = 0x48;
		*inst_cursor++ = 0xB8;
		for (int i = 0; i < 8; ++i) *inst_cursor++ = ((unsigned char*)&(unsigned __int64){ (unsigned __int64)Output })[i];
		// mov [rsp], rax
		*inst_cursor++ = 0x48;
		*inst_cursor++ = 0x89;
		*inst_cursor++ = 0x04;
		*inst_cursor++ = 0x24;
		// mov rax, Input
		*inst_cursor++ = 0x48;
		*inst_cursor++ = 0xB8;
		for (int i = 0; i < 8; ++i) *inst_cursor++ = ((unsigned char*)&(unsigned __int64){ (unsigned __int64)Input })[i];
		// mov [rsp+8], rax
		*inst_cursor++ = 0x48;
		*inst_cursor++ = 0x89;
		*inst_cursor++ = 0x44;
		*inst_cursor++ = 0x24;
		*inst_cursor++ = 0x08;
		// xor r13, r13
		*inst_cursor++ = 0x4D;
		*inst_cursor++ = 0x31;
		*inst_cursor++ = 0xED;

		bool encountered_errors = false;
		for (char* scan = source_code;; ++scan)
		{
			while ((unsigned char)(*scan-1) < (unsigned char)0x20) ++scan;
			if (*scan == 0) break;

			switch (*scan)
			{
				case '>':
				{
					// add r13d, 1
					*inst_cursor++ = 0x66;
					*inst_cursor++ = 0x41;
					*inst_cursor++ = 0x83;
					*inst_cursor++ = 0xC5;
					*inst_cursor++ = 0x01;
				} break;

				case '<':
				{
					// sub r13d, 1
					*inst_cursor++ = 0x66;
					*inst_cursor++ = 0x41;
					*inst_cursor++ = 0x83;
					*inst_cursor++ = 0xED;
					*inst_cursor++ = 0x01;
				} break;

				case '+':
				{
					// add BYTE [r12 + r13], 1
					*inst_cursor++ = 0x43;
					*inst_cursor++ = 0x80;
					*inst_cursor++ = 0x04;
					*inst_cursor++ = 0x2C;
					*inst_cursor++ = 0x01;
				} break;

				case '-':
				{
					// sub BYTE [r12 + r13], 1
					*inst_cursor++ = 0x43;
					*inst_cursor++ = 0x80;
					*inst_cursor++ = 0x2C;
					*inst_cursor++ = 0x2C;
					*inst_cursor++ = 0x01;
				} break;

				case '.':
				{
					// mov cl, [r12 + r13]
					*inst_cursor++ = 0x43;
					*inst_cursor++ = 0x8A;
					*inst_cursor++ = 0x0C;
					*inst_cursor++ = 0x2C;
					// sub rsp, 16
					*inst_cursor++ = 0x48;
					*inst_cursor++ = 0x83;
					*inst_cursor++ = 0xEC;
					*inst_cursor++ = 0x10;
					// call [rsp]
					*inst_cursor++ = 0xFF;
					*inst_cursor++ = 0x54;
					*inst_cursor++ = 0x24;
					*inst_cursor++ = 0x10;
					// add rsp, 16
					*inst_cursor++ = 0x48;
					*inst_cursor++ = 0x83;
					*inst_cursor++ = 0xC4;
					*inst_cursor++ = 0x10;
				} break;

				case ',':
				{
					// call [rsp + 8]
					*inst_cursor++ = 0xFF;
					*inst_cursor++ = 0x54;
					*inst_cursor++ = 0x24;
					*inst_cursor++ = 0x08;
					// mov [r12 + r13], al
					*inst_cursor++ = 0x43;
					*inst_cursor++ = 0x88;
					*inst_cursor++ = 0x04;
					*inst_cursor++ = 0x2C;
				} break;

				case '[':
				{
					// cmp BYTE [r12 + r13], 0
					*inst_cursor++ = 0x43;
					*inst_cursor++ = 0x80;
					*inst_cursor++ = 0x3C;
					*inst_cursor++ = 0x2C;
					*inst_cursor++ = 0x00;
					// je ----
					*inst_cursor++ = 0x0F;
					*inst_cursor++ = 0x84;

					assert(bracket_stack_size < sizeof(bracket_stack)/sizeof(bracket_stack[0]));
					bracket_stack[bracket_stack_size++] = inst_cursor;
					inst_cursor += 4;
				} break;

				case ']':
				{
					// cmp BYTE [r12 + r13], 0
					*inst_cursor++ = 0x43;
					*inst_cursor++ = 0x80;
					*inst_cursor++ = 0x3C;
					*inst_cursor++ = 0x2C;
					*inst_cursor++ = 0x00;
					// jne ----
					*inst_cursor++ = 0x0F;
					*inst_cursor++ = 0x85;
					
					assert(bracket_stack_size > 0);
					char* head = bracket_stack[--bracket_stack_size];

					assert(inst_cursor-head <= 0x7FFFFFFF);
					int fwd_displacement = (int)(inst_cursor-head);
					int bwd_displacement = -fwd_displacement;

					for (int i = 0; i < 4; ++i) *inst_cursor++ = ((char*)&bwd_displacement)[i];

					for (int i = 0; i < 4; ++i) *head++ = ((char*)&fwd_displacement)[i];

				} break;

				default:
				{
					fprintf(stderr, "Illegal character in program: %c\n", *scan);
					encountered_errors = true;
				} break;
			}
		}

		if (!encountered_errors)
		{
			// NOTE: epilogue
			// add rsp, 16
			*inst_cursor++ = 0x48;
			*inst_cursor++ = 0x83;
			*inst_cursor++ = 0xC4;
			*inst_cursor++ = 0x10;
			// ret
			*inst_cursor++ = 0xC3;

			// TODO: instruction buffer size
			BOOL succeeded = VirtualProtect(inst, 0xFFFF, PAGE_EXECUTE_READ, &(DWORD){0});
			assert(succeeded);

			void (*program)(void* data) = inst;
			program(data);
		}
	}

	return 0;
}
