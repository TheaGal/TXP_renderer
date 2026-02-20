from pathlib import Path

CMAKELISTS_FNAME = './CMakeLists.txt'


def find_existing_files(search_dirs: list[str], search_extensions: list[str]) -> list[Path]:
    
    all_found_files: list[Path] = []
    for search_dir in search_dirs:
        for search_ext in search_extensions:
            # Convert `search_ext` to case-insensitive extension.
            case_insensitive_ext = '*.'
            for ext_char in search_ext:
                case_insensitive_ext += f'[{ext_char.lower()}{ext_char.upper()}]'

            # Search directory for extension.
            files = list(Path(search_dir).rglob(case_insensitive_ext))
            all_found_files.extend(files)

    return all_found_files


def prepend_src_file_entry(fname: Path) -> str:
    return '    ${CMAKE_CURRENT_SOURCE_DIR}/' + f'{fname.as_posix()}'


def read_existing_surrounding_src_entries_strings(start_block: str, end_block: str) -> tuple[str, str]:
    before_str_block = ''
    after_str_block = ''

    block_process = 0  # 0:before; 1:within; 2:after;
    with open(CMAKELISTS_FNAME, 'r') as f:
        for line in f:
            if block_process == 0:
                # Add before str line.
                before_str_block += line

                if line.strip() == start_block:
                    # Found start of block.
                    block_process = 1
            elif block_process == 1:
                if line.strip() == end_block:
                    # Add after str line.
                    after_str_block += line

                    # Found end of block.
                    block_process = 2
            elif block_process == 2:
                # Add after str line.
                after_str_block += line

    return before_str_block, after_str_block


def update_file_entries(existing_file_search_dirs: list[str],
                        search_extensions: list[str],
                        cmakelists_start_block: str,
                        cmakelists_end_block: str):
    existing_files = find_existing_files(existing_file_search_dirs, search_extensions)
    src_entries = [prepend_src_file_entry(x) for x in existing_files]
    src_entries.sort()
    src_entries_str_block = '\n'.join(src_entries) + '\n'  # Add newline at end of block.

    before_str_block, after_str_block = \
        read_existing_surrounding_src_entries_strings(cmakelists_start_block,
                                                      cmakelists_end_block)

    complete_str_data = before_str_block + src_entries_str_block + after_str_block
    
    with open(CMAKELISTS_FNAME, 'w') as f:
        f.write(complete_str_data)


if __name__ == '__main__':
    # C++ source files.
    CPP_SEARCH_EXTENSIONS = ['h',
                             'hpp',
                             'ixx',  # I think this is for modules???
                             'c',
                             'cxx',
                             'cpp']
    update_file_entries(['./include/', './src/'],
                        CPP_SEARCH_EXTENSIONS,
                        'set(MAIN_SOURCES',
                        ')')
    # Resource files.
    RES_SEARCH_EXTENSIONS = ['ktx2',
                             'wobj',  # wavefront obj files (it's .wobj bc well .obj makes the compiler think it's an object file (NOT GOOD))
                             'mtl',   # wavefront obj material file.
                             'glb',
                             'gltf',
                             'shader',
                             'shadrefl']
    update_file_entries(['./assets/',],
                        RES_SEARCH_EXTENSIONS,
                        'set(ASSET_DIR_FILES',
                        ')')
