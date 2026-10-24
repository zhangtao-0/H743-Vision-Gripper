"""CDC selection regression; no real serial port is opened."""
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from start_viewer import find_port

def port(name,vid,pid):
    return SimpleNamespace(device=name,vid=vid,pid=pid,description=name)

class PortSelection(unittest.TestCase):
    def test_native_only(self):
        with patch('serial.tools.list_ports.comports',return_value=[port('COM9',0x1A86,0x7523),port('COM11',0x0483,0x5740)]):
            self.assertEqual(find_port(),'COM11')
            self.assertEqual(find_port('com11'),'com11')
    def test_no_guess_or_missing_explicit(self):
        with patch('serial.tools.list_ports.comports',return_value=[port('COM9',0x1A86,0x7523)]):
            with self.assertRaises(RuntimeError):find_port()
            with self.assertRaises(RuntimeError):find_port('COM11')
    def test_multiple_native_requires_choice(self):
        with patch('serial.tools.list_ports.comports',return_value=[port('COM11',0x0483,0x5740),port('COM12',0x0483,0x5740)]):
            with self.assertRaises(RuntimeError):find_port()
            self.assertEqual(find_port('COM12'),'COM12')

if __name__=='__main__':unittest.main()
