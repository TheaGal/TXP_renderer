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
                              # @THEA: turning on obfuscate caused a weird error in __debug_color_grad_line.slang when loading.
                              #   Here is the error below:
                              #   program_source:48:91: error: illegal vector component name 'd'
                              #   out.gl_Position = (float4(in.m_5.xyz, 1.0) * float4x4(float4(entryPointParams._m0->_m1.data[0].x, entryPointParams._m0->_m1.data[1].x, entryPointParams._m0->_m1.data[2].x, entryPointParams._m0->_m1.data[3].x), float4(entryPointParams._m0->_m1.data[0].y, entryPointParams._m0->_m1.data[1].y, entryPointParams._m0->_m1.data[2].y, entryPointParams._m0->_m1.data[3].y), float4(entryPointParams._m0->_m1.data[0].z, entryPointParams._m0->_m1.data[1].z, entryPointParams._m0->_m1.data[2].z, entryPointParams._m0->_m1.data[3].z), float4(entryPointParams._m0->_m1.data[0].w, entryPointParams._m0->_m1.data[1].w, entryPointParams._m0->_m1.data[2].w, entryPointParams._m0->_m1.data[3].w))) * float4x4(float4(entryPointParams._m0->_m0.data[0].x, entryPointParams._m0->_m0.data[1].x, entryPointParams._m0->_m0.data[2].x, entryPointParams._m0->_m0.data[3].x), float4(entryPointParams._m0->_m0.data[0].y, entryPointParams._m0->_m0.data[1].y, entryPointParams._m0->_m0.data[2].y, entryPointParams._m0->_m0.data[3].y), float4(entryPointParams._m0->_m0.data[0].z, entryPointParams._m0->_m0.data[1].z, entryPointParams._m0->_m0.data[2].z, entryPointParams._m0->_m0.data[3].z), float4(entryPointParams._m0->_m0.data[0].w, entryPointParams._m0->_m0.data[1].w, entryPointParams._m0->_m0.data[2].w, entryPointParams._m0->_m0.data[3].w));
                              #                                                                                         ^~~~~
                              #   Perhaps, later you can try updating the slang compiler or play w settings to try re-obfuscating the source code?
                              #   Or, just not worry about obfuscating the shader.
                            #   '-obfuscate',
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
            if len(proc_shader_compile.stdout) > 0:
                print(f'\n    {proc_shader_compile.stdout}')
        else:
            print(f'\n    {proc_shader_compile.stderr}')
