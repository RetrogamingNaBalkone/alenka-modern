from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class EnglishCourseTests(unittest.TestCase):
    def test_course_progression_and_structure(self):
        raw = (ROOT / 'resources/lessons-en.txt').read_bytes()
        lines = raw.decode('ascii').splitlines()
        original = (ROOT / 'original/ALENKA/ALLE.TXT').read_bytes().decode('cp866').split('\r\n')[1:]
        self.assertEqual(len(lines), 163)
        self.assertEqual(lines[141], '')
        self.assertEqual(lines[-1], '')
        self.assertTrue(raw.replace(b'\r\n', b'\n').endswith(b'\n\n'))
        self.assertTrue(all(len(line) <= 70 for line in lines))
        self.assertTrue(all(32 <= ord(c) <= 126 for line in lines for c in line))
        mapping = str.maketrans('ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ', "QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,.")
        drills = (1,2,3,4,5,6,7,10,12,14,16,18,23,25,27,29,35,37,39)
        allowed = {' '}
        for n, line in enumerate(lines[:43], 1):
            if n in drills:
                self.assertEqual(line, original[n-1].translate(mapping))
                allowed.update(line)
            else:
                self.assertFalse(set(line) - allowed, f'Unintroduced keys in line {n}')
        for n in (92,93,95,97,99,101,103):
            self.assertEqual(lines[n-1], original[n-1])
        for line in lines[43:91]:
            words = line.split(' ')
            self.assertEqual(len(words), 12)
            self.assertEqual(len(set(words)), 1)
            self.assertEqual(len(words[0]), 4)
        self.assertTrue(lines[142].startswith('The weary Mole also'))
        self.assertTrue(lines[-2].endswith('for the same simple welcome.'))

if __name__ == '__main__':
    unittest.main()
