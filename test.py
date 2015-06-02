#!/usr/bin/env python
import re
import sys
import unittest
import subprocess


class Regex(object):
    def __init__(self, pattern):
        self.pattern = pattern


def assert_matches(result, expected):
    if isinstance(expected, Regex):
        if not re.match(expected.pattern, result.strip()):
            raise AssertionError('{} !=~ {}'.format(result.strip(), expected.pattern))
    else:
        if expected.strip() != result.strip():
            raise AssertionError('{} != {}'.format(result.strip(), expected.strip()))


class TestAcceptance(object):
    def __init__(self):
        if sys.platform == 'darwin':
            self.platform = 'osx'
        elif sys.platform.startswith('linux'):
            self.platform = 'linux'
        else:
            assert False

    def run(self, name, result=None, build_error=None, runtime_error=None, input_file=None):
        build_cmd = '{}/build.sh {}'.format(self.platform, name)
        build_proc = subprocess.Popen(build_cmd, shell=True, stderr=subprocess.PIPE)

        if build_error:
            assert_matches(build_proc.stderr.read(), build_error)
            assert build_proc.wait() != 0
            return
        else:
            assert build_proc.wait() == 0

        if input_file:
            run_cmd = 'build/{} < {}'.format(name, input_file)
        else:
            run_cmd = 'build/{}'.format(name)

        run_proc = subprocess.Popen(run_cmd, shell=True, stderr=subprocess.PIPE, stdout=subprocess.PIPE)

        if runtime_error:
            assert_matches(run_proc.stderr.read(), runtime_error)
            assert run_proc.wait() != 0
        else:
            assert_matches(run_proc.stdout.read(), result)
            assert run_proc.wait() == 0

    # Fast tests ( < 100ms)
    def test_fast(self):
        self.run('euler1', result='233168')
        self.run('euler2', result='4613732')
        self.run('euler3', result='6857')
        self.run('euler5', result='232792560')
        self.run('euler6', result='25164150')
        self.run('euler7', result='104743')
        self.run('euler8', result='40824')
        self.run('euler9', result='31875000')
        self.run('euler11', result='70600674')
        self.run('euler13', result='5537376230', input_file='testing/euler13.txt')
        self.run('euler15', result='137846528820')
        self.run('euler16', result='1366')
        self.run('euler17', result='21124')
        self.run('euler18', result='1074', input_file='testing/euler18.txt')
        self.run('euler20', result='648')
        self.run('euler21', result='31626')
        #self.run('euler22', result='871198282', input_file='testing/names.txt')
        self.run('poly', result='4')
        self.run('poly2', result='False')
        self.run('fail', runtime_error='*** Exception: Called head on empty list')
        self.run('unaryMinus', result='-5')
        self.run('localVar', result='3')
        self.run('unnamed', result='19')
        self.run('typeConstructor', result='7')
        self.run('associativity', result='10')
        self.run('closure', result='7')
        self.run('structMembers1', build_error='Error: Near line 3, column 5: symbol "x" is already defined')
        self.run('structMembers2', build_error='Error: Near line 7, column 5: symbol "y" is already defined')
        self.run('structMembers3', build_error='Error: Near line 5, column 1: symbol "x" is already defined in this scope')
        self.run('structInference', result='7')
        #self.run('bigList', result='')
        self.run('multiRef', result='6')
        self.run('shortCircuit', result='Hello')
        self.run('localopt', result='22')
        self.run('functionArg', result='12')
        self.run('functionArg2', result='12')
        self.run('importSemantic', build_error='Error: Near line 4, column 1: error: cannot unify types Bool and Int')
        self.run('syntaxError', build_error='Error: Near line 1, column 2: expected tEOL, but got =')
        self.run('constructorMismatch', build_error='Error: Near line 3, column 10: Expected 1 parameter(s) to type constructor MyPair, but got 2')
        self.run('overrideType', build_error=Regex('Error: Near line 2, column 16: error: cannot unify types Int and a\d+'))
        self.run('noReturn', result='1')
        self.run('noReturn2', build_error='Error: Near line 1, column 0: error: cannot unify types Unit and Int')
        self.run('noReturn3', build_error='Error: Near line 1, column 0: error: cannot unify types Unit and Int')
        self.run('implicitReturn', result='4')
        self.run('implicitReturn2', result='10')
        self.run('fizzBuzz', result=open('testing/fizzBuzz.correct').read())
        self.run('adt1', result='5')
        self.run('adt2', result='2')
        self.run('adt3', build_error='Error: Near line 6, column 5: cannot repeat constructors in match statement')
        self.run('adt4', build_error='Error: Near line 4, column 1: switch statement is not exhaustive')

    # Medium tests (100ms-1s)
    def test_medium(self):
        self.run('euler4', result='906609')
        self.run('euler12', result='76576500')
        self.run('euler19', result='171')
        self.run('euler19-2', result='171')
        self.run('euler26', result='983')
        self.run('euler25', result='4782')

    # Slow tests (> 1s)
    def test_slow(self):
        self.run('euler10', result='142913828922')
        self.run('euler14', result='837799')
        self.run('euler23', result='4179871')
        self.run('euler24', result='2783915460')
        self.run('euler27', result='-59231')

