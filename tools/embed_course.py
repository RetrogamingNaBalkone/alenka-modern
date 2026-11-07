"""Validate and embed the English course and native-resolution button lettering."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def generate(destination):
    text = (ROOT / 'resources/lessons-en.txt').read_text(encoding='ascii')
    lines = text.splitlines()
    if not lines or any(len(line) > 70 for line in lines):
        raise ValueError('English course must contain lines of at most 70 characters')
    if any(not 32 <= ord(c) <= 126 for line in lines for c in line):
        raise ValueError('English course contains a control character')
    glyphs = (ROOT / 'resources/eng-button.txt').read_text(encoding='ascii').splitlines()
    if len(glyphs) != 12 or any(len(row) != 23 or set(row) - {'.', '#'} for row in glyphs):
        raise ValueError('ENG button must be a 23 by 12 monochrome bitmap')
    content = '#pragma once\nnamespace alenka {\n'
    content += 'inline constexpr const char* english_lines[] = {\n'
    for line in lines:
        content += '    "' + line.replace('\\', '\\\\').replace('"', '\\"').replace('?', '\\?') + '",\n'
    content += '};\ninline constexpr const char* eng_button[] = {\n'
    content += ''.join('    "' + row + '",\n' for row in glyphs)
    content += '};\n}\n'
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text() != content:
        destination.write_text(content, encoding='ascii')
    print(f'Embedded English course: {len(lines)} lines / {sum(map(len, lines))} characters')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination')
    generate(parser.parse_args().destination)
