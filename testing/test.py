#!/usr/bin/env python
import re
import sys
import subprocess


if sys.platform == 'darwin':
    platform = 'osx'
elif sys.platform.startswith('linux'):
    platform = 'linux'
else:
    assert False


class Regex(object):
    def __init__(self, pattern):
        self.pattern = pattern


def matches(result, expected):
    if isinstance(expected, Regex):
        return re.match(expected.pattern, result.strip())
    else:
        return expected.strip() == result.strip()


def run_test(name, result=None, build_error=None, runtime_error=None, input_file=None):
    build_cmd = '{}/build.sh {}'.format(platform, name)
    build_proc = subprocess.Popen(build_cmd, shell=True, stderr=subprocess.PIPE)

    if build_error:
        assert matches(build_proc.stderr.read(), build_error)
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
        assert matches(run_proc.stderr.read(), runtime_error)
        assert run_proc.wait() != 0
    else:
        assert matches(run_proc.stdout.read(), result)
        assert run_proc.wait() == 0


# Fast tests ( < 100ms)
run_test('euler1', result='233168')
run_test('euler2', result='4613732')
run_test('euler3', result='6857')
run_test('euler5', result='232792560')
run_test('euler6', result='25164150')
run_test('euler7', result='104743')
run_test('euler8', result='40824')
run_test('euler9', result='31875000')
run_test('euler11', result='70600674')
run_test('euler13', result='5537376230', input_file='testing/euler13.txt')
run_test('euler15', result='137846528820')
run_test('euler16', result='1366')
run_test('euler17', result='21124')
run_test('euler18', result='1074', input_file='testing/euler18.txt')
run_test('euler20', result='648')
run_test('euler21', result='31626')
run_test('euler22', result='871198282', input_file='testing/names.txt')
run_test('poly', result='4')
run_test('poly2', result='False')
run_test('fail', runtime_error='*** Exception: Called head on empty list')
run_test('unaryMinus', result='-5')
run_test('localVar', result='3')
run_test('unnamed', result='19')
run_test('typeConstructor', result='7')
run_test('associativity', result='10')
run_test('closure', result='7')
run_test('structMembers1', build_error='Error: Near line 3, column 5: symbol "x" is already defined')
run_test('structMembers2', build_error='Error: Near line 7, column 5: symbol "y" is already defined')
run_test('structMembers3', build_error='Error: Near line 5, column 1: symbol "x" is already defined in this scope')
run_test('structInference', result='7')
run_test('bigList', result='')
run_test('multiRef', result='6')
run_test('shortCircuit', result='Hello')
run_test('localopt', result='22')
run_test('functionArg', result='12')
run_test('functionArg2', result='12')
run_test('importSemantic', build_error='Error: Near line 4, column 1: error: cannot unify types Bool and Int')
run_test('syntaxError', build_error='Error: Near line 1, column 2: expected tEOL, but got =')
run_test('constructorMismatch', build_error='Error: Near line 3, column 10: Expected 1 parameter(s) to type constructor MyPair, but got 2')
run_test('overrideType', build_error=Regex('Error: Near line 2, column 16: error: cannot unify types Int and a\d+'))
run_test('noReturn', result='1')
run_test('noReturn2', build_error='Error: Near line 1, column 0: error: cannot unify types Unit and Int')
run_test('noReturn3', build_error='Error: Near line 1, column 0: error: cannot unify types Unit and Int')
run_test('implicitReturn', result='4')
run_test('implicitReturn2', result='10')
run_test('fizzBuzz', result=open('testing/fizzBuzz.correct').read())
run_test('adt1', result='5')
run_test('adt2', result='2')
run_test('adt3', build_error='Error: Near line 6, column 5: cannot repeat constructors in match statement')
run_test('adt4', build_error='Error: Near line 4, column 1: switch statement is not exhaustive')

# Medium tests (100ms-1s)
run_test('euler4', result='906609')
run_test('euler12', '76576500')
run_test('euler19', '171')
run_test('euler19-2', '171')
run_test('euler26', '983')
run_test('euler25', '4782')

# Slow tests (> 1s)
run_test('euler10', '142913828922')
run_test('euler14', '837799')
run_test('euler23', '4179871')
run_test('euler24', '2783915460')
run_test('euler27', '-59231')

