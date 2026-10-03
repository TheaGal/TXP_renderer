import os
from pathlib import Path
from random import choice
from string import ascii_lowercase
import subprocess
import sys
from typing import List, Tuple
import imageio.v3 as imgio
import numpy as np
import yaml  # @NOTE: may have to run `pip install pyyaml`


SOURCE_DIR = './assets_raw/textures'
SOURCE_RECIPE_PATH = f'{SOURCE_DIR}/_textures_recipe.yaml'
BUILD_DIR = './assets/textures'


def get_exec_list(format_str: str,
                  transform_to_linear: bool,
                  is_mipmapped: bool,
                  source_image: str,
                  output_path: str) -> List[str]:
    exec_list = ['ktx',
                 'create',
                 '--format', format_str,
                 '--fail-on-color-conversions',
                 '--fail-on-origin-changes',
                 source_image,
                 output_path]

    if transform_to_linear:
        exec_list.extend(['--assign-tf', 'linear'])

    if is_mipmapped:
        exec_list.extend(['--generate-mipmap'])

    return exec_list


def gen_format_string(num_channels: int, format: str) -> Tuple[str, bool]:
    assert num_channels in [1, 2, 4]  # 3 channel textures are not supported in the engine.

    format_bits = ''
    format_suffix = ''
    is_linear = False
    if format == 'ldr_unorm':
        format_bits = '8'
        format_suffix = 'UNORM'
        is_linear = True
    elif format == 'ldr_srgb':
        format_bits = '8'
        format_suffix = 'SRGB'
    elif format == 'hdr':
        format_bits = '8'
        format_suffix = 'SFLOAT'
        assert False, 'idk here if it\'s linear or not'

    full_format_str = ''
    if num_channels >= 1:
        full_format_str += f'R{format_bits}'
    if num_channels >= 2:
        full_format_str += f'G{format_bits}'
    if num_channels >= 3:
        full_format_str += f'B{format_bits}'
    if num_channels >= 4:
        full_format_str += f'A{format_bits}'
    full_format_str += f'_{format_suffix}'

    return full_format_str, is_linear


def build_texture2d(num_channels: int,
                    format: str,
                    is_mipmapped: bool,
                    source_image: str,
                    output_path: str):
    format_str, transform_to_linear = gen_format_string(num_channels, format)
    exec_list = get_exec_list(format_str,
                              transform_to_linear,
                              is_mipmapped,
                              f'{SOURCE_DIR}/{source_image}',
                              output_path)
    ret_code = subprocess.call(exec_list)
    if ret_code != 0:
        print('-' * 80)
        print(exec_list)
        print('FAILED from command above ^^')
        sys.exit(1)


def build_texture2d_channelmapped(num_channels: int,
                                  format: str,
                                  is_mipmapped: bool,
                                  source_imgs: List[str],
                                  output_path: str):
    assert num_channels > 1
    assert len(source_imgs) == num_channels

    write_img_w = -1
    write_img_h = -1

    src_fill_values: List[float] = []

    # Fill in image data.
    for src_img in source_imgs:
        try:
            # Try interpreting the image path as a number.
            poss_fill_value = float(src_img)
            src_fill_values.append(poss_fill_value)

        except ValueError:
            src_fill_values.append(-1)  # -1 means image is used instead of fill value.

            # Load in the image path and get image details.
            src_img_data = imgio.imread(Path(SOURCE_DIR) / src_img)  # pyright: ignore[reportUnknownVariableType, reportUnknownMemberType]
            src_img_w = len(src_img_data)                            # pyright: ignore[reportUnknownArgumentType]
            src_img_h = len(src_img_data[0])
            src_img_channels = 1
            try:
                src_img_channels = len(src_img_data[0][0])
            except TypeError:
                pass

            # Write image details.
            if write_img_w < 0:
                # Set the dimensions of write image.
                write_img_w = src_img_w
                write_img_h = src_img_h

                # Create write image to write to.
                write_img_data = np.zeros((src_img_w, src_img_h, num_channels), dtype=np.uint8)
            else:
                # Ensure dimensions are compatible.
                if write_img_w != src_img_w or write_img_h != src_img_h:
                    raise ValueError(
                        f'Dimensions don\'t match: ({write_img_w} x {write_img_h}) vs ' \
                        f'({src_img_w} x {src_img_h})')

            # Write data to write image.
            assert len(src_fill_values) >= 1
            current_channel_idx = len(src_fill_values) - 1

            for x in range(write_img_w):
                for y in range(write_img_h):
                    pix_data = src_img_data[x][y]
                    if src_img_channels > 1:
                        pix_data = pix_data[0]  # Use R channel only.

                    write_img_data[x][y][current_channel_idx] = pix_data  # pyright: ignore[reportPossiblyUnboundVariable]

    # Fill in fill values.
    assert num_channels == len(src_fill_values)

    for chan in range(num_channels):
        fill_val = src_fill_values[chan]
        if fill_val >= 0.0:
            for x in range(write_img_w):
                for y in range(write_img_h):
                    write_img_data[x][y][chan] = fill_val * 255       # pyright: ignore[reportPossiblyUnboundVariable]

    # Write image.
    temp_img_fname = ''
    while len(temp_img_fname) == 0 or (Path(SOURCE_DIR) / temp_img_fname).exists():
        temp_img_fname = f'{"".join(choice(ascii_lowercase) for _ in range(16))}.png'

    temp_img_path = Path(SOURCE_DIR) / temp_img_fname

    imgio.imwrite(temp_img_path, write_img_data)  # pyright: ignore[reportUnknownMemberType, reportPossiblyUnboundVariable]

    # Use intermediate temp image.
    build_texture2d(num_channels, format, is_mipmapped, temp_img_fname, output_path)

    # Delete intermediate image.
    os.remove(temp_img_path)


if __name__ == '__main__':
    # Get recipes.
    recipes = {}
    with open(SOURCE_RECIPE_PATH) as stream:
        try:
            recipes = yaml.safe_load(stream)
        except yaml.YAMLError as exc:
            print(exc)

    # Build each texture.
    for texture_recipe_name in recipes.keys():                # type: ignore
        # Building message.
        building_texture_msg = f'Building texture \"{texture_recipe_name}\"'
        print(f'{building_texture_msg:<60} ... ', end='', flush=True)

        # Gather recipe info.
        texture_recipe = recipes[texture_recipe_name]         # type: ignore
        texture_recipe_type = next(iter(texture_recipe))      # type: ignore
        recipe_content = texture_recipe[texture_recipe_type]  # type: ignore

        output_path = f'{BUILD_DIR}/{texture_recipe_name}.ktx2'

        if texture_recipe_type == 'Texture2D':

            num_channels: int = recipe_content['channels']            # type: ignore
            format: str = recipe_content['format']                    # type: ignore
            is_mipmapped: bool = recipe_content['mipmapped']            # type: ignore
            source_image: str = recipe_content['source_image']            # type: ignore

            build_texture2d(num_channels,  # type: ignore
                            format,        # type: ignore
                            is_mipmapped,  # type: ignore
                            source_image,  # type: ignore
                            output_path)

        elif texture_recipe_type == 'Texture2D_channelmapped':

            num_channels: int = recipe_content['channels']                          # type: ignore
            format: str = recipe_content['format']                                  # type: ignore
            is_mipmapped: bool = recipe_content['mipmapped']                        # type: ignore
            source_imgs: List[str] = []
            if num_channels >= 1:
                source_imgs.append(recipe_content['source_image_r'])            # type: ignore
            if num_channels >= 2:
                source_imgs.append(recipe_content['source_image_g'])            # type: ignore
            if num_channels >= 3:
                source_imgs.append(recipe_content['source_image_b'])            # type: ignore
            if num_channels >= 4:
                source_imgs.append(recipe_content['source_image_a'])            # type: ignore

            build_texture2d_channelmapped(num_channels,  # type: ignore
                                          format,        # type: ignore
                                          is_mipmapped,  # type: ignore
                                          source_imgs,
                                          output_path)

        else:
            raise ValueError(f'unknown recipe type: {texture_recipe_type}')

        # Complete!
        print('done')
