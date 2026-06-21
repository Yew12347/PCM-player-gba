"""
Copyright (c) 2022 laqieer laqieer@126.com
zlib License, see LICENSE file.
"""

# Doc on BMFont: https://angelcode.com/products/bmfont/doc/file_format.html

import os
import sys
import shlex
import codecs
import argparse
import traceback
import subprocess

from file_info import FileInfo
from PIL import Image

trim_fonts = False
unique_characters = {

    # ================= 1. ASCII (95) =================
    ' ','!','"','#','$','%','&',"'","(",")","*",'+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',
    ':',';','<','=','>','?','@',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '[','\\',']','^','_','`',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '{','|','}','~',

    # ================= 2. PUNCTUATION & MATH (45) =================
    '–','—','…','·','×','÷','≠','≤','≥','±','°',
    '、','。','「','」','『','』','〜','・',
    '？','！','：','；','｀','＾','￣','＿','ー','～',
    '（','）','［','］','｛','｝','〈','〉','《','》',
    '【','】','〔','〕','＼','＋','＝','￠','￡','￥',

    # ================= 3. UI / MEDIA ICONS & GEOMETRIC SHAPES (45) =================
    '▶','■','⏸','⏭','⏮','⏩','⏪','⏯',
    '↑','↓','←','→',
    '✔','✖','★','☆','◆','●','○',
    '▲','▼','◀','◆','◇','◊','▽','△','◼','◻',
    '♠','♣','♥','♦','➕','➖','✖','➗','❓','❗',
    '⭕','❌','❔','❕','🌀','🎵','🎶','🔔','🔕',

    # ================= 4. STANDARD KANA (110) =================
    'ア','イ','ウ','エ','オ','カ','キ','ク','ケ','コ',
    'サ','シ','ス','セ','ソ','タ','チ','ツ','テ','ト',
    'ナ','ニ','ヌ','ネ','ノ','ハ','ヒ','フ','ヘ','ホ',
    'マ','ミ','ム','メ','モ','ヤ','ユ','ヨ',
    'ラ','リ','ル','レ','ロ','ワ','ヲ','ン',
    'ァ','ィ','ゥ','ェ','ォ','ッ','ャ','ュ','ョ','ー',
    'あ','い','う','え','お','か','き','く','け','こ',
    'さ','し','す','せ','そ','た','ち','つ','て','と',
    'な','に','ぬ','ね','の','は','ひ','ふ','へ','ほ',
    'ま','み','む','め','も','や','ゆ','よ',
    'ら','り','ル','れ','ろ','わ','を','ん',
    'ぁ','ぃ','ぅ','ぇ','ぉ','っ','ゃ','ゅ','ょ',

    # ================= 5. JAPANESE DAKUTEN / VOICED KANA (72) =================
    'が','ぎ','ぐ','げ','ご','ざ','じ','ず','ぜ','ぞ',
    'だ','ぢ','づ','で','ど','ば','び','ぶ','べ','ぼ',
    'ぱ','ぴ','ぷ','ぺ','ぽ',
    'ガ','ギ','グ','ゲ','ゴ','ザ','ジ','ズ','ゼ','ゾ',
    'ダ','ヂ','ヅ','デ','ド','バ','ビ','ブ','ベ','ボ',
    'パ','ピ','プ','ペ','ポ','ヴ','ッ','ン', 
    'ェ','ォ','ャ','ュ','ョ','ワ','ヰ','ヱ','ヲ', 

    # ================= 6. ORIGINAL KANJI POOL (79) =================
    '日','月','火','水','木','金','土','年','時','分','秒',
    '文','字','名','称','設','定','情','報','画','面','表','示',
    '開','閉','読','書','削','除','作','成','保','存','編','集',
    '選','決','確','認','戻','次','終','了','始',
    '音','曲','再','生','停','止','一','前','量','速','遅',
    'フ','ル','デ','ム','プ', 
    '人','大','小','中','上','下','左','右','心',
    '見','聞','行','来','出','入','使','知','思',
    '新','古','多','少','高','低','良','悪','長','短',

    # ================= 7. EXTENDED KANJI POOL (562) =================
    '全','部','分','数','値','計','算','変','更','リ','ト', 
    '目','次','通','知','語','言','英','言','本','国','連',
    '接','切','替','機','能','用','紙','出','力','入','力',
    '電','源','気','点','検','索','結','果','手','動','自',
    '動','送','信','受','信','発','送','返','事','宛','先',
    '何','学','校','先','生','学','男','女','子','父','母',
    '友','達','家','族','屋','店','買','物','食','飲','料',
    '理','茶','酒','昼','晩','朝','夜','今','週','間','毎',
    '回','方','北','南','東','西','京','都','道','府','県',
    '市','町','村','場','所','駅','車','道','旅','空','港',
    '海','川','山','森','林','天','気','雨','雪','風','晴',
    '詠', # UTF-8: 35424
    '敏', # UTF-8: 25937
    '敐', '救', '主',
    '物','鳥','犬','猫','馬','魚','肉','米','麦','油','塩',
    '内','外','側','間','横','端','角','隅','対','面','背',
    '線','点','円','丸','交','通','路','橋','街','道','畑',
    '強','弱','重','軽','暗','明','暗','太','細','厚','薄',
    '空','港','雲','星','雷','光','氷','岩','石','砂','泥',
    '頭','顔','目','耳','鼻','口','歯','首','声','手','足',
    '体','骨','肉','肌','毛','血','指','爪','肩','胸','腰',
    '心','情','感','愛','恋','恨','怒','悲','喜','楽','苦',
    '笑','泣','痛','疲','眠','静','動','忙','暇','健','康',
    '病','医','薬','院','死','生','活','命','老','若','死',
    '私','僕','俺','君','彼','彼女','誰','何','私','己','自',
    '世','界','地','球','国','島','都','市','区','有','無',
    '社','会','人','会','組','織','団','体','委','員','長',
    '官','庁','政','治','法','律','規','則','契','約','法',
    '警','察','裁','判','罪','犯','盗','破','危','嫌','安',
    '全','防','犯','火','災','地','震','津','波','風','水',
    '買','売','商','店','品','質','価','格','費','用','税',
    '円','貨','幣','銀','行','金','庫','貯','金','利','息',
    '産','業','工','場','製','品','農','業','漁','業','林',
    '労','働','雇','用','職','業','事','務','管','理','職',
    '開','発','企','画','技','術','研','究','実','験','テ',
    '教','育','教科','書','宿','題','試','験','成','績','卒',
    '文','学','歴','史','地','理','科','学','数','学','物',
    '化','学','生','物','天','文','哲','学','宗','教','神',
    '仏','寺','神','社','教','会','聖','書','経','典','祈',
    '芸','術','美','術','絵','画','彫','刻','写','真','映',
    '音','楽','歌','謡','舞','踊','劇','団','演','技','映',
    '放','送','番','組','新','聞','雑','誌','出','版','著',
    '旅','行','観','光','旅','館','ホ','テ','ル','旅','券',
    '遊','園','地','映','画','館','博','物','館','美','術',
    '運','動','ス','ポ','ー','ツ','野','球','サ','ッ','カ',
    '陸','上','水泳','体','操','柔','道','剣','道','弓',
    '衣','類','服','装','洋','服','和','服','着','物','靴',
    '鞄','帽','子','傘','布','糸','針','綿','絹','皮','革',
    '住','居','住','宅','建','物','部','屋','玄','関','台',
    '風','呂','便','所','庭','壁','柱','床','天','井','窓',
    '家','具','机','椅','子','棚','鏡','寝','具','布','団',
    '道','具','器','具','機','械','部','品','材','料','木',
    '鉄','鋼','金','属','銅','銀','鉛','亜','鉛','硫','黄',
    '過','去','現','在','将','来','未','来','今','度','昔',
    '春','夏','秋','冬','季','節','朝','昼','晩','夕','方',
    '曜','日','週','末','月','末','年','末','年','始','平',
    '数','字','百','千','万','億','兆','零','幾','何','学',
    '形','状','直','線','曲','線','平','行','垂','直','直',
    '角','三','角','四','角','五','角','多','角','長','方',
    '正','方','菱','形','楕','円','球','体','円','柱','角',
    '立','方','体','体','積','面','積','周','囲','長','さ',
    '幅','高','さ','深','さ','厚','み','密','度','重','さ',
    '量','容','量','温','度','熱','量','気','圧','風','速',
    '東','西','南','北','方','角','方','向','左','右','前',
    '後','上','下','内','外','中','央','中','心','周','辺',
    '遠','近','近','所','附','近','離','島','辺','境','国',

    # ================= 8. FULL-WIDTH ALPHANUMERIC (62) =================
    '０','１','２','３','４','５','６','７','８','９',
    'Ａ','Ｂ','Ｃ','Ｄ','Ｅ','Ｆ','Ｇ','Ｈ','Ｉ','Ｊ','Ｋ','著','Ｍ',
    'Ｎ','Ｏ','Ｐ','Ｑ','Ｒ','Ｓ','T','Ｕ','Ｖ','Ｗ','Ｘ','Ｙ','Ｚ',
    'ａ','ｂ','ｃ','ｄ','ｅ','ｆ','ｇ','ｈ','ｉ','ｊ','ｋ','ｌ','ｍ',
    'ｎ','ｏ','ｐ','ｑ','ｒ','ｓ','ｔ','ｕ','ｖ','ｗ','ｘ','ｙ','ｚ',

    # ================= 9. THAI SUPPORT (FULL SET: 179) =================
    # Consonants (44)
    'ก','ข','ฃ','ค','ฅ','ฆ','ง','จ','ฉ','ช','ซ','ฌ','ญ','ฎ','ฏ',
    'ฐ','ฑ','ฒ','ณ','ด','ต','ถ','ท','ธ','น','บ','ป','ผ','ฝ','พ',
    'ฟ','ภ','ม','ย','ร','ฤ','ล','ฦ','ว','ศ','ษ','ส','ห','ฬ',
    'อ','ฮ',

    # Vowels & Modifiers (Regular, Combined, Ober/Under-scripts)
    'ะ','ั','า','ำ','ิ','ี','ึ','ื','ุ','ู','ฺ',
    'เ','แ','โ','ใ','ไ','ๅ','ๆ','็','ํ','์','ํ',

    # Tone Marks
    '่','้','๊','๋',

    # Thai Numerals (0-9)
    '๐','๑','๒','๓','๔','๕','๖','๗','๘','๙',

    # Special Symbols & Currency
    '฿','ฯ','ะ','ั','ๆ','์','๎','๏','๚','๛'
}

def list_texts_files(texts_paths):
    global trim_fonts
    texts_file_names = []
    texts_file_paths = []

    if texts_paths is not None:
        texts_path_list = texts_paths.split(' ')

        for texts_path in texts_path_list:
            if os.path.isfile(texts_path):
                texts_file_name = os.path.basename(texts_path)
                if FileInfo.validate(texts_file_name):
                    texts_file_names.append(texts_file_name)
                    texts_file_paths.append(texts_path)
            elif os.path.isdir(texts_path):
                folder_texts_file_names = sorted(os.listdir(texts_path))
                for texts_file_name in folder_texts_file_names:
                    texts_file_path = texts_path + '/' + texts_file_name

                    if os.path.isfile(texts_file_path) and FileInfo.validate(texts_file_name):
                        texts_file_names.append(texts_file_name)
                        texts_file_paths.append(texts_file_path)

    trim_fonts = len(texts_file_names) > 0

    return texts_file_names, texts_file_paths


def process_texts_files(texts_file_paths, characters_file_path):
    global unique_characters
    for texts_file_path in texts_file_paths:
        texts_file_ext = os.path.splitext(texts_file_path.lower())[1]
        if texts_file_ext in ('.c', '.cpp', '.cc', '.cxx', '.c++', '.h', '.hpp', '.hh', '.hxx', '.h++', '.s', '.inc', '.asm'):
            if os.environ.get('DEVKITARM'):
                text = subprocess.check_output([os.environ['DEVKITARM'] + '/bin/arm-none-eabi-cpp', '-w', '-fpreprocessed', texts_file_path]).decode('utf-8')
            elif os.environ.get('WONDERFUL_TOOLCHAIN'):
                text = subprocess.check_output([os.environ['WONDERFUL_TOOLCHAIN'] + '/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-cpp', '-w', '-fpreprocessed', texts_file_path]).decode('utf-8')
            else:
                raise Exception('DEVKITARM and WONDERFUL_TOOLCHAIN not found')
            for char in text:
                unique_characters.add(char)
            continue
        with open(texts_file_path, 'r', encoding='utf-8') as texts_file:
            for line in texts_file:
                for char in line:
                    unique_characters.add(char)
    with open(characters_file_path, 'w', encoding='utf-8') as characters_file:
        for char in sorted(unique_characters):
            characters_file.write(char)


def list_fonts_files(fonts_folder_paths):
    fonts_folder_path_list = fonts_folder_paths.split(' ')
    fonts_file_names = []
    fonts_file_paths = []

    for fonts_folder_path in fonts_folder_path_list:
        folder_fonts_file_names = sorted(os.listdir(fonts_folder_path))

        for fonts_file_name in folder_fonts_file_names:
            if fonts_file_name.endswith('.fnt'):
                fonts_file_path = fonts_folder_path + '/' + fonts_file_name

                if os.path.isfile(fonts_file_path) and FileInfo.validate(fonts_file_name):
                    fonts_file_names.append(fonts_file_name)
                    fonts_file_paths.append(fonts_file_path)

    return fonts_file_names, fonts_file_paths


def process_fonts_files(fonts_file_paths, build_folder_path):
    global trim_fonts, unique_characters
    fonts_graphics_path = build_folder_path + '/fonts/'
    if not os.path.exists(fonts_graphics_path):
        os.makedirs(fonts_graphics_path)
    total_number = 0
    for fonts_file_path in fonts_file_paths:
        fonts_file_path_no_ext = os.path.splitext(fonts_file_path)[0]
        fonts_folder_path, fonts_file_name_no_ext = os.path.split(fonts_file_path_no_ext)
        font_name = fonts_file_name_no_ext + '_sprite_font'
        fonts_header_path = build_folder_path + '/' + font_name + '.h'

        with open(fonts_file_path, 'r', encoding='utf-8') as fonts_file, open(fonts_header_path, 'w', encoding='utf-8') as header_file:
            font_chars = []
            font_widths = [0] * 95
            unique_chars = unique_characters.copy()

            for fonts_line in fonts_file:
                line_type, *pair_tokens = shlex.split(fonts_line)
                line_conf = dict(pair_token.split("=", 1) for pair_token in pair_tokens)

                if line_type == "info":
                    padding_up, padding_right, padding_down, padding_left = [int(x) for x in line_conf['padding'].split(',')]
                    header_file.write('// ' + line_conf['face'].replace('"', '') + ' ' + line_conf['size'] + 'px')
                    if line_conf['bold'] != '0':
                        header_file.write(' bold')
                    if line_conf['italic'] != '0':
                        header_file.write(' italic')
                    header_file.write('\n')
                    header_file.write('\n')
                elif line_type == "common":
                    font_height = int(line_conf['lineHeight'])
                    if font_height > 64:
                        raise ValueError('Font is too large')
                    elif font_height > 32:
                        font_height = 64
                    elif font_height > 16:
                        font_height = 32
                    elif font_height > 8:
                        font_height = 16
                    else:
                        font_height = 8
                    font_base = int(line_conf['base'])
                    font_y_offset = min(font_base - font_height, 0)
                    # Assume a font's width is not more than its height
                    font_width = font_height
                    font_pages = [None] * int(line_conf['pages'])
                elif line_type == "page":
                    page_file_path = fonts_folder_path + '/' + line_conf['file'].replace('"', '')
                    font_pages[int(line_conf['id'])] = Image.open(page_file_path)
                elif line_type == "chars":
                    font_number = int(line_conf['count'])
                    #fonts_image = Image.new('RGBA', (font_width, font_height * font_number))
                    transparent_color = font_pages[0].getpixel((0, 0))
                    fonts_image = Image.new('RGB', (font_width, font_height * (font_number + 94)), transparent_color)
                elif line_type == "char":
                    if len(unique_characters) == 0:
                        break
                    font_code = int(line_conf['id'])
                    if not trim_fonts or chr(font_code) in unique_chars:
                        if trim_fonts:
                            unique_chars.remove(chr(font_code))
                        src_left = int(line_conf['x']) + padding_left
                        src_upper = int(line_conf['y']) + padding_up
                        src_right = int(line_conf['x']) + int(line_conf['width']) - padding_right
                        if src_right < src_left:
                            src_right = src_left
                        src_right = min(src_right, src_left + font_width)
                        src_lower = int(line_conf['y']) + int(line_conf['height']) - padding_down
                        if src_lower < src_upper:
                            src_lower = src_upper
                        src_upper = max(src_upper, src_lower - font_height)
                        dst_left = round(float(line_conf['xoffset'])) + padding_left
                        if dst_left < 0:
                            dst_left = 0
                        dst_right = dst_left + src_right - src_left
                        if dst_right > font_width:
                            dst_left -= min(dst_left, dst_right - font_width)
                        dst_upper = round(float(line_conf['yoffset'])) + padding_up
                        if dst_upper < 0:
                            dst_upper = 0
                        dst_lower = dst_upper + src_lower - src_upper
                        if dst_lower > font_height:
                            dst_upper -= min(dst_upper, dst_lower - font_height)
                        if dst_lower > 0:
                            dst_lower -= min(dst_lower, font_y_offset)
                        font_w = max(int(line_conf['xadvance']), int(line_conf['width']) - padding_left -padding_right)
                        font_w = min(font_w, font_width)
                        if font_code > 126:
                            font_chars.append(chr(font_code))
                            font_widths.append(font_w)
                            dst_upper += font_height * (len(font_widths) - 2)
                        elif font_code > 31:
                            font_widths[font_code - 32] = font_w
                            dst_upper += font_height * (font_code - 33)
                        if font_code > 32:
                            fonts_image.paste(font_pages[int(line_conf['page'])].crop((src_left, src_upper, src_right, src_lower)), (dst_left, dst_upper))

            header_file.write('#ifndef ' + font_name.upper() + '_H\n')
            header_file.write('#define ' + font_name.upper() + '_H\n')
            header_file.write('\n')
            header_file.write('#include "bn_sprite_font.h"\n')
            header_file.write('#include "bn_utf8_characters_map.h"\n')
            header_file.write('#include "bn_sprite_items_' + fonts_file_name_no_ext + '.h"\n')
            header_file.write('\n')
            if len(font_chars) > 0:
                header_file.write('constexpr bn::utf8_character ' + font_name + '_utf8_characters[] = {\n')
                header_file.write('    "' + '", "'.join(font_chars) + '"\n')
                header_file.write('};\n')
                header_file.write('\n')
                header_file.write('constexpr bn::span<const bn::utf8_character> ' + font_name + '_utf8_characters_span(\n')
                header_file.write('        ' + font_name + '_utf8_characters);\n')
                header_file.write('\n')
                header_file.write('constexpr auto ' + font_name + '_utf8_characters_map =\n')
                header_file.write('        bn::utf8_characters_map<' + font_name + '_utf8_characters_span>();\n')
                header_file.write('\n')
            header_file.write('constexpr int8_t ' + font_name + '_character_widths[] = {\n')
            header_file.write('    ' + ', '.join([str(x) for x in font_widths]) + '\n')
            header_file.write('};\n')
            header_file.write('\n')
            header_file.write('constexpr bn::sprite_font ' + font_name + '(\n')
            header_file.write('        bn::sprite_items::' + fonts_file_name_no_ext + ',\n')
            if len(font_chars) > 0:
                header_file.write('        ' + font_name + '_utf8_characters_map.reference(),\n')
            else:
                header_file.write('        bn::utf8_characters_map_ref(),\n')
            header_file.write('        ' + font_name + '_character_widths);\n')
            header_file.write('\n')
            header_file.write('#endif')
            fonts_image_path_no_ext = fonts_graphics_path + fonts_file_name_no_ext
            font_number = len(font_widths) - 1
            total_number += font_number
            fonts_image_trimmed = Image.new('RGB', (font_width, font_height * total_number), transparent_color)
            fonts_image_trimmed.paste(fonts_image)
            #fonts_image_trimmed.save(fonts_image_path_no_ext + '.png')
            fonts_image_trimmed = fonts_image_trimmed.convert("P", palette=Image.Palette.ADAPTIVE, colors=16)
            transparent_color_index = fonts_image_trimmed.getpixel((0, 0))
            if transparent_color_index > 0:
                dest_map = list(range(16))
                dest_map[0], dest_map[transparent_color_index] = transparent_color_index, 0
                fonts_image_trimmed = fonts_image_trimmed.remap_palette(dest_map)
            fonts_image_trimmed.save(fonts_image_path_no_ext + '.bmp')
            with open(fonts_image_path_no_ext + '.json', 'w') as json_file:
                json_file.write('{\n')
                json_file.write('    "type": "sprite",\n')
                json_file.write('    "height": ' + str(font_height) + '\n')
                json_file.write('}\n')
            print('    ' + fonts_file_path + ' font header written in ' + fonts_header_path + ' (character number: ' + str(font_number) + ')')

    return total_number


def process_fonts(fonts_folder_paths, build_folder_path, texts_paths):
    texts_file_names, texts_file_paths = list_texts_files(texts_paths)
    text_file_info_path = build_folder_path + '/_bn_texts_files_info.txt'
    old_text_file_info = FileInfo.read(text_file_info_path)
    new_text_file_info = FileInfo.build_from_files(texts_file_paths)

    fonts_file_names, fonts_file_paths = list_fonts_files(fonts_folder_paths)
    font_file_info_path = build_folder_path + '/_bn_fonts_files_info.txt'
    old_font_file_info = FileInfo.read(font_file_info_path)
    new_font_file_info = FileInfo.build_from_files(fonts_file_paths)

    if old_font_file_info == new_font_file_info and old_text_file_info == new_text_file_info:
        return

    characters_file_path = build_folder_path + '/_bn_characters.txt'
    old_characters = FileInfo.read(characters_file_path)
    process_texts_files(texts_file_paths, characters_file_path)
    new_characters = FileInfo.read(characters_file_path)

    if old_font_file_info == new_font_file_info and old_characters == new_characters:
        return

    for fonts_file_name in fonts_file_names:
        print(fonts_file_name)

    sys.stdout.flush()

    total_number = process_fonts_files(fonts_file_paths, build_folder_path)
    print('    Processed character number: ' + str(total_number))
    new_font_file_info.write(font_file_info_path)
    new_text_file_info.write(text_file_info_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Butano fonts tool.')
    parser.add_argument('--build', required=True, help='build folder path')
    parser.add_argument('--fonts', required=True, help='fonts folder paths')
    parser.add_argument('--texts', required=False, help='texts folder or files paths')

    try:
        args = parser.parse_args()
        process_fonts(args.fonts, args.build, args.texts)
    except Exception as ex:
        sys.stderr.write('Error: ' + str(ex) + '\n')
        traceback.print_exc()
        exit(-1)
