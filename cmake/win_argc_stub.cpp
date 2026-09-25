/*
 * win_argc_stub.cpp — Compilacion cruzada con LLVM-MinGW (clang).
 *
 * libQt6EntryPoint.a de los builds oficiales de Qt (MinGW) espera los
 * simbolos dllimport del CRT __imp___argc / __imp___argv / __imp___wargv.
 * La toolchain clang de LLVM-MinGW no los provee (su CRT accede a la linea
 * de comandos por __p___argc), asi que el enlazador falla con
 * "undefined symbol: __imp___argc". Este stub define esos simbolos
 * apuntando a datos locales. main() de esta aplicacion no usa argc/argv,
 * por lo que el EntryPoint de Qt (que llama main(argc, argv)) funciona
 * con los valores nulos sin problema.
 *
 * Nota: debe ser .cpp (no .c) porque el proyecto declara LANGUAGES CXX;
 * CMake descarta fuentes de idiomas no habilitados en silencio.
 */
extern "C" {
int _crudqt_stub_argc = 0;
void *_crudqt_stub_argv = 0;
void *_crudqt_stub_wargv = 0;
}

__asm__(".globl __imp___argc\n.set __imp___argc, _crudqt_stub_argc\n"
        ".globl __imp___argv\n.set __imp___argv, _crudqt_stub_argv\n"
        ".globl __imp___wargv\n.set __imp___wargv, _crudqt_stub_wargv\n");
