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
    def run(self, name, result=None, build_error=None, runtime_error=None, input_file=None):
        build_cmd = './sbuild {}'.format(name)
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

    def test_euler1(self):
        self.run('euler1', result='233168')

    def test_euler2(self):
        self.run('euler2', result='4613732')

    def test_euler3(self):
        self.run('euler3', result='6857')

    def test_euler5(self):
        self.run('euler5', result='232792560')

    def test_euler6(self):
        self.run('euler6', result='25164150')

    def test_euler7(self):
        self.run('euler7', result='104743')

    def test_euler8(self):
        self.run('euler8', result='40824')

    def test_euler9(self):
        self.run('euler9', result='31875000')

    def test_euler11(self):
        self.run('euler11', result='70600674')

    def test_euler13(self):
        self.run('euler13', result='5537376230', input_file='testing/euler13.txt')

    def test_euler15(self):
        self.run('euler15', result='137846528820')

    def test_euler16(self):
        self.run('euler16', result='1366')

    def test_euler17(self):
        self.run('euler17', result='21124')

    def test_euler18(self):
        self.run('euler18', result='1074', input_file='testing/euler18.txt')

    def test_euler20(self):
        self.run('euler20', result='648')

    def test_euler21(self):
        self.run('euler21', result='31626')

    def test_euler22(self):
        self.run('euler22', result='871198282', input_file='testing/names.txt')

    def test_euler24(self):
        self.run('euler24', result='2783915460')

    def test_poly(self):
        self.run('poly', result='4')

    def test_poly2(self):
        self.run('poly2', result='False')

    def test_fail(self):
        self.run('fail', runtime_error='*** Exception: Called head on empty list')

    def test_unaryMinus(self):
        self.run('unaryMinus', result='-5')

    def test_localVar(self):
        self.run('localVar', result='3')

    def test_unnamed(self):
        self.run('unnamed', result='9')

    def test_typeConstructor(self):
        self.run('typeConstructor', result='7')

    def test_associativity(self):
        self.run('associativity', result='10')

    def test_closure(self):
        self.run('closure', result='7')

    def test_structMembers1(self):
        self.run('structMembers1', build_error='Error: testing/structMembers1.spl:1:1: type "Test" already has a member named "x"')

    def test_structMembers2(self):
        self.run('structMembers2', result='')

    def test_structMembers3(self):
        self.run('structMembers3', result='')

    def test_structInference(self):
        self.run('structInference', result='7')

    def test_bigList(self):
        self.run('bigList', result='')

    def test_multiRef(self):
        self.run('multiRef', result='6')

    def test_shortCircuit(self):
        self.run('shortCircuit', result='Hello')

    def test_localopt(self):
        self.run('localopt', result='22')

    def test_functionArg(self):
        self.run('functionArg', result='12')

    def test_functionArg2(self):
        self.run('functionArg2', result='12')

    def test_importSemantic(self):
        self.run('importSemantic', build_error='Error: testing/importSemantic.spl:4:1: cannot unify types Int and Bool')

    def test_syntaxError(self):
        self.run('syntaxError', build_error='Error: testing/syntaxError.spl:1:1: left-hand side of assignment statement is not an lvalue')

    def test_constructorMismatch(self):
        self.run('constructorMismatch', build_error='Error: testing/constructorMismatch.spl:3:10: Expected 1 parameter(s) to type constructor MyPair, but got 2')

    def test_overrideType(self):
        self.run('overrideType', build_error='Error: testing/overrideType.spl:2:16: cannot unify types Int and T')

    def test_noReturn(self):
        self.run('noReturn', result='1')

    def test_noReturn2(self):
        self.run('noReturn2', build_error='Error: testing/noReturn2.spl:1:1: cannot unify types Unit and Int')

    def test_noReturn3(self):
        self.run('noReturn3', build_error='Error: testing/noReturn3.spl:1:1: cannot unify types Unit and Int')

    def test_implicitReturn(self):
        self.run('implicitReturn', result='4')

    def test_implicitReturn2(self):
        self.run('implicitReturn2', result='10')

    def test_fizzBuzz(self):
        self.run('fizzBuzz', result=open('testing/fizzBuzz.correct').read())

    def test_adt1(self):
        self.run('adt1', result='5')

    def test_adt2(self):
        self.run('adt2', result='2')

    def test_adt3(self):
        self.run('adt3', build_error='Error: testing/adt3.spl:7:5: cannot repeat constructors in match statement')

    def test_adt4(self):
        self.run('adt4', build_error='Error: testing/adt4.spl:4:1: switch statement is not exhaustive')

    def test_array1(self):
        self.run('array1', result='12345')

    def test_array2(self):
        self.run('array2', result='15')

    def test_array3(self):
        self.run('array3', result='')

    def test_intOutOfRange1(self):
        self.run('intOutOfRange1', build_error='Error: testing/intOutOfRange1.spl:2:6: error: integer literal out of range: 9223372036854775808')

    def test_intOutOfRange2(self):
        self.run('intOutOfRange2', build_error='Error: testing/intOutOfRange2.spl:2:6: error: integer literal out of range: -9223372036854775809')

    def test_wrongReturnType(self):
        self.run('wrongReturnType', build_error='Error: testing/wrongReturnType.spl:2:5: cannot unify types String and Int')

    def test_mainStackRoots(self):
        self.run('mainStackRoots', result='1')

    def test_scopes1(self):
        self.run('scopes1', build_error='Error: testing/scopes1.spl:4:14: symbol "x" is not defined in this scope')

    def test_scopes2(self):
        self.run('scopes2', build_error='Error: testing/scopes2.spl:6:14: symbol "x" is not defined in this scope')

    def test_scopes3(self):
        self.run('scopes3', build_error='Error: testing/scopes3.spl:4:14: symbol "x" is not defined in this scope')

    def test_scopes4(self):
        self.run('scopes4', build_error='Error: testing/scopes4.spl:5:14: symbol "x" is not defined in this scope')

    def test_scopes5(self):
        self.run('scopes5', result='1')

    def test_scopes6(self):
        self.run('scopes6', result='1\n2\n3\n4')

    def test_scopes7(self):
        self.run('scopes7', result='success')

    def test_let_repeated_var(self):
        self.run('let_repeated_var', result='0')

    def test_euler28(self):
        self.run('euler28', result='669171001')

    def test_order_of_operations(self):
        self.run('order_of_operations', '')

    def test_method(self):
        self.run('method', 'Different')

    def test_method2(self):
        self.run('method2', 'Same')

    def test_method3(self):
        self.run('method3', 'Different')

    def test_method4(self):
        self.run('method4', '3')

    def test_method5(self):
        self.run('method5', build_error='Error: testing/method5.spl:11:5: type "[Int]" already has a method or member named "myAt"')

    def test_map(self):
        self.run('map', result='3')

    def test_repeatedTypeParam(self):
        self.run('repeatedTypeParam', build_error='Error: testing/repeatedTypeParam.spl:1:1: type parameter "T" is already defined')

    def test_repeatedTypeParam2(self):
        self.run('repeatedTypeParam2', build_error='Error: testing/repeatedTypeParam2.spl:1:1: type parameter "T" is already defined')

    def test_repeatedTypeParam3(self):
        self.run('repeatedTypeParam3', build_error='Error: testing/repeatedTypeParam3.spl:1:1: type parameter "T" is already defined')

    def test_repeatedTypeParam4(self):
        self.run('repeatedTypeParam4', build_error='Error: testing/repeatedTypeParam4.spl:2:5: type parameter "T" is already defined')

    def test_repeatedTypeParam5(self):
        self.run('repeatedTypeParam5', build_error='Error: testing/repeatedTypeParam5.spl:2:5: type parameter "T" is already defined')

    def test_repeatedTypeParam6(self):
        self.run('repeatedTypeParam6', build_error='Error: testing/repeatedTypeParam6.spl:1:1: type parameter "T" is already defined')

    def test_memberMethodConflict(self):
        self.run('memberMethodConflict', build_error='Error: testing/memberMethodConflict.spl:5:5: type "Test" already has a method or member named "f"')

    def test_memberAsMethod(self):
        self.run('memberAsMethod', build_error='Error: testing/memberAsMethod.spl:5:1: "test" is a member variable, not a method')

    def test_methodAsMember(self):
        self.run('methodAsMember', build_error='Error: testing/methodAsMember.spl:9:1: "doSomething" is a method, not a member variable')

    def test_structMemberNames(self):
        self.run('structMemberNames', result='30\nFrance')

    def test_vector(self):
        self.run('vector', result='37')

    def test_assignToFunction(self):
        self.run('assignToFunction', build_error='Error: testing/assignToFunction.spl:4:1: symbol "f" is not a variable')

    def test_mutateStruct(self):
        self.run('mutateStruct', result='4\n5')

    def test_mututallyRecursiveMethods(self):
        self.run('mutuallyRecursiveMethods', '0')

    def test_unreachable(self):
        self.run('unreachable', '4')

    def test_hashTable(self):
        self.run('hashTable', '20')

    def test_hashTable2(self):
        self.run('hashTable2', '20')

    def test_hashTable3(self):
        self.run('hashTable3', '2')

    def test_foreignClosure(self):
        self.run('foreignClosure', build_error='Error: testing/foreignClosure.spl:1:6: Cannot put external function "strHash" into a closure')

    def test_constructorClosure(self):
        self.run('constructorClosure', '3')

    def test_genericChain2(self):
        self.run('genericChain2', '4')

    def test_genericChain3(self):
        self.run('genericChain3', '4')


    # Medium tests (100ms-1s)

    def test_euler4(self):
        self.run('euler4', result='906609')

    def test_euler10(self):
        self.run('euler10', result='142913828922')

    def test_euler12(self):
        self.run('euler12', result='76576500')

    def test_euler19(self):
        self.run('euler19', result='171')

    def test_euler26(self):
        self.run('euler26', result='983')

    def test_euler25(self):
        self.run('euler25', result='4782')

    def test_euler30(self):
        self.run('euler30', result='443839')

    # Slow tests (> 1s)

    def test_euler14(self):
        self.run('euler14', result='837799')

    def test_euler23(self):
        self.run('euler23', result='4179871')

    def test_euler27(self):
        self.run('euler27', result='-59231')

    def test_euler29(self):
        self.run('euler29', result='9183')