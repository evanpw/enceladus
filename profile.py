#!/usr/bin/env python
import re
import sys
import time
import subprocess


if sys.platform == 'darwin':
    platform = 'osx'
elif sys.platform.startswith('linux'):
    platform = 'linux'
else:
    assert False


class TestCase(object):
    def __init__(self, name, input_file=None):
        self.name = name
        self.input_file = input_file


    def build(self):
        build_cmd = '{}/build.sh {}'.format(platform, self.name)
        build_proc = subprocess.Popen(build_cmd, shell=True, stderr=subprocess.PIPE)

        assert build_proc.wait() == 0

    def run(self):
        if self.input_file:
            run_cmd = 'build/{} < {}'.format(self.name, self.input_file)
        else:
            run_cmd = 'build/{}'.format(self.name)

        run_proc = subprocess.Popen(run_cmd, shell=True, stderr=subprocess.PIPE, stdout=subprocess.PIPE)

        assert run_proc.wait() == 0


tests = []

def add(name, input_file=None):
    test = TestCase(name, input_file)
    test.build()
    tests.append(test)


add('euler4')
add('euler12', )
add('euler19',)
add('euler19-2')
add('euler26')
add('euler25')
add('euler10')
add('euler14')
add('euler23')
add('euler24')
add('euler27')


def get_time(test):
    time1 = time.time()
    test.run()
    time2 = time.time()

    print '{}: {:0.3f}s'.format(test.name, time2 - time1)


start = time.time()
for test in tests:
    get_time(test)
end = time.time()

print 'Total elapsed time: {:0.3f}s'.format(end - start)
