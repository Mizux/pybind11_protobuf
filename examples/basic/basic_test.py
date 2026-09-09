import sys
import unittest
import basic

class TestBasic(unittest.TestCase):
  def test_add(self):
    self.assertEqual(basic.add(1, 2), 3)

if __name__ == "__main__":
  print(f"Runtime Python version: {sys.version}")
  unittest.main()
