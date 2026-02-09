from pathlib import Path

CMAKELISTS_FNAME = './CMakeLists.txt'
START_BLOCK = 'set(MAIN_SOURCES'
END_BLOCK = ')'


def find_existing_files() -> list[Path]:
    SEARCH_DIRS = ['./include/', './src/']
    SEARCH_EXTENSIONS = ['h',
                         'hpp',
                         'ixx',  # I think this is for modules???
                         'c',
                         'cxx',
                         'cpp']
    all_found_files: list[Path] = []
    for search_dir in SEARCH_DIRS:
        for search_ext in SEARCH_EXTENSIONS:
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


def read_existing_surrounding_src_entries_strings() -> tuple[str, str]:
    before_str_block = ''
    after_str_block = ''

    block_process = 0  # 0:before; 1:within; 2:after;
    found_source_files = []
    with open(CMAKELISTS_FNAME, 'r') as f:
        for line in f:
            if block_process == 0:
                # Add before str line.
                before_str_block += line

                if line.strip() == START_BLOCK:
                    # Found start of block.
                    block_process = 1
            elif block_process == 1:
                if line.strip() == END_BLOCK:
                    # Add after str line.
                    after_str_block += line

                    # Found end of block.
                    block_process = 2
            elif block_process == 2:
                # Add after str line.
                after_str_block += line

    return before_str_block, after_str_block


if __name__ == '__main__':
    existing_files = find_existing_files()
    src_entries = [prepend_src_file_entry(x) for x in existing_files]
    src_entries.sort()
    src_entries_str_block = '\n'.join(src_entries) + '\n'  # Add newline at end of block.

    before_str_block, after_str_block = read_existing_surrounding_src_entries_strings()

    complete_str_data = before_str_block + src_entries_str_block + after_str_block
    
    with open(CMAKELISTS_FNAME, 'w') as f:
        f.write(complete_str_data)
