from pathlib import Path
import subprocess


SOURCE_DIR = './assets_raw/shaders'
SOURCE_FILE_EXT = '*.slang'
BUILD_DIR = './assets/shaders'


if __name__ == '__main__':
    # Get all files.
    files = list(Path(SOURCE_DIR).rglob(SOURCE_FILE_EXT))

    for file in files:
        relative_fpath = file.relative_to(SOURCE_DIR)

        src_fname = file.as_posix()
        build_fname = (Path(BUILD_DIR) / relative_fpath.with_suffix('.shader')).as_posix()
        reflect_fname = (Path(BUILD_DIR) / relative_fpath.with_suffix('.shadrefl')).as_posix()

        shader_compile_cmd = ['slangc',
                              '-lang slang',
                              '-profile spirv_1_5',
                              '-target spirv',
                              f'-reflection-json {reflect_fname}',
                              '-O2',
                              '-obfuscate',
                              '-fvk-use-entrypoint-name',
                              f'{src_fname}',
                              '>',
                              f'{build_fname}']

        compile_str = f'Compile \"{relative_fpath.as_posix()}\" ... '
        print(compile_str, end='')

        COMPILE_STR_COLUMN_LENGTH = 70

        proc_shader_compile = subprocess.run(' '.join(shader_compile_cmd),
                                             shell=True,
                                             capture_output=True)        

        if proc_shader_compile.returncode == 0:
            print((' ' * max(0, COMPILE_STR_COLUMN_LENGTH - len(compile_str))) + 'SUCCESS!')
        else:
            print(f'\n    {proc_shader_compile.stderr}')
