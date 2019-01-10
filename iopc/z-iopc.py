#!/usr/bin/env python
#vim:set fileencoding=utf-8:
##########################################################################
#                                                                        #
#  Copyright (C) INTERSEC SA                                             #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

import os, os.path

SELF_PATH = os.path.dirname(__file__)
TEST_PATH = os.path.join(SELF_PATH, 'testsuite')
IOPC = os.path.join(SELF_PATH, 'iopc')

import z
import subprocess

@z.ZGroup
class IopcTest(z.TestCase):

    def run_iopc(self, iop, expect_pass, errors, version=1, lang='',
                 class_id_range=''):
        iopc_args = [IOPC, os.path.join(TEST_PATH, iop)]
        # enable features of v2
        if version == 2:
            iopc_args.append('--features-v2')
        if version == 3:
            iopc_args.append('--features-v3')
        if version == 4:
            iopc_args.append('--features-v4')
        # in case of expected success if no language is specified
        # the success must be for all the languages
        if expect_pass and not lang:
            lang = 'C,json'
        # specific language(s)
        if lang:
            iopc_args.append('-l' + lang)
        if class_id_range:
            iopc_args.append('--class-id-range')
            iopc_args.append(class_id_range)
        iopc_p = subprocess.Popen(iopc_args, stderr=subprocess.PIPE)
        self.assertIsNotNone(iopc_p)
        output = iopc_p.communicate()[1]
        if (expect_pass):
            self.assertEqual(iopc_p.returncode, 0,
                             "unexpected failure on %s: %s" % (iop, output))
        else:
            self.assertTrue(iopc_p.returncode > 0, "unexpected pass on %s"
                            % (iop))
        if (errors):
            if isinstance(errors, basestring):
                errors = [errors]
            for error in errors:
                self.assertTrue(output.find(error) >= 0,
                                "did not find '%s' in '%s'" \
                                % (error, output))

    def run_iopc_pass(self, iop, version, lang='', class_id_range=''):
        self.run_iopc(iop, True, None, version, lang, class_id_range)

    # compilation will fail with or without --features-v2
    def run_iopc_fail(self, iop, errors, lang='', class_id_range=''):
        self.run_iopc(iop, False, errors, 1, lang, class_id_range)
        self.run_iopc(iop, False, errors, 2, lang, class_id_range)
        self.run_iopc(iop, False, errors, 3, lang, class_id_range)

    # iop must be an iop file using v2 features,
    # i.e. compiling without -2 / --features-v2 will fail
    def run_iopc2(self, iop, expect_pass, errors, lang='', class_id_range=''):
        self.run_iopc(iop, False, 'use -2 param', 1, lang, class_id_range)
        self.run_iopc(iop, expect_pass, errors, 2, lang, class_id_range)

    # iop must be an iop file using v3 features,
    # i.e. compiling without -3 / --features-v3 will fail
    def run_iopc3(self, iop, expect_pass, errors, lang='', class_id_range=''):
        self.run_iopc(iop, False, 'use -3 param', 1, lang, class_id_range)
        self.run_iopc(iop, False, 'use -3 param', 2, lang, class_id_range)
        self.run_iopc(iop, expect_pass, errors, 3, lang, class_id_range)

    def run_gcc(self, iop):
        gcc_args = ['gcc', '-c', '-o', '/dev/null', '-std=gnu99',
                    '-O', '-Wall', '-Werror',
                    '-D_GNU_SOURCE',
                    '-I' + os.path.join(SELF_PATH, '../lib-common/compat'),
                    '-I' + os.path.join(SELF_PATH, '..'),
                    os.path.join(TEST_PATH, iop + '.c') ]
        gcc_p = subprocess.Popen(gcc_args)
        self.assertIsNotNone(gcc_p)
        gcc_p.wait()
        self.assertEqual(gcc_p.returncode, 0)

    def check_file(self, file_name, string_list, wanted = True):
        f = open(os.path.join(TEST_PATH, file_name))
        self.assertIsNotNone(f)
        content = f.read()
        for s in string_list:
            if wanted:
                self.assertTrue(content.find(s) >= 0,
                                "did not find '%s' in '%s'" \
                                % (s, file_name))
            else:
                self.assertTrue(content.find(s) < 0,
                                "found '%s' in '%s'" \
                                % (s, file_name))
        f.close()

    def check_ref(self, pkg, lang):
        self.assertEqual(subprocess.call(['diff', '-u',
                                          pkg + '.iop.' + lang,
                                          pkg + '.ref.' + lang]), 0)

    def test_circular_type_valid(self):
        f = 'circular_type_valid.iop'
        self.run_iopc_pass(f, 1)
        self.run_gcc(f)
        self.run_iopc_pass(f, 2)
        self.run_gcc(f)
        self.run_iopc_pass(f, 3)
        self.run_gcc(f)

    def test_circular_type_invalid(self):
        self.run_iopc_fail('circular_type_invalid.iop', 'circular dependency')

    def test_circular_pkg_valid(self):
        self.run_iopc_pass('pkg_a.iop', 1)
        self.run_iopc_pass('pkg_b.iop', 1)
        self.run_iopc_pass('pkg_c.iop', 1)
        self.run_gcc('pkg_a.iop')
        self.run_gcc('pkg_b.iop')
        self.run_gcc('pkg_c.iop')

        self.run_iopc_pass('pkg_a.iop', 2)
        self.run_iopc_pass('pkg_b.iop', 2)
        self.run_iopc_pass('pkg_c.iop', 2)
        self.run_gcc('pkg_a.iop')
        self.run_gcc('pkg_b.iop')
        self.run_gcc('pkg_c.iop')

        self.run_iopc_pass('pkg_a.iop', 3)
        self.run_iopc_pass('pkg_b.iop', 3)
        self.run_iopc_pass('pkg_c.iop', 3)
        self.run_gcc('pkg_a.iop')
        self.run_gcc('pkg_b.iop')
        self.run_gcc('pkg_c.iop')

    def test_circular_pkg_invalid(self):
        self.run_iopc_fail('circular_pkg_a.iop', 'circular dependency')
        self.run_iopc_fail('circular_pkg_b.iop', 'circular dependency')
        self.run_iopc_fail('circular_pkg_c.iop', 'circular dependency')

    def test_attrs_valid(self):
        f = 'attrs_valid.iop'
        self.run_iopc_pass(f, 3)
        self.run_gcc(f)

    def test_attrs_multi_valid(self):
        f = 'attrs_multi_valid.iop'
        self.run_iopc_pass(f, 3)
        self.run_gcc(f)
        path_base = os.path.join(TEST_PATH,
                                 'attrs_multi_valid.iop.c')
        path_ref = os.path.join(TEST_PATH,
                                'reference_attrs_multi_valid.c')
        ref_base = open(path_base, "r")
        ref = open(path_ref, "r")
        self.assertEqual(ref.read(), ref_base.read())

    def test_attrs_multi_constraints(self):
        f = 'attrs_multi_constraints.iop'
        self.run_iopc_pass(f, 3)
        self.run_gcc(f)
        path_base = os.path.join(TEST_PATH, 'attrs_multi_constraints.iop.c')
        path_ref = os.path.join(TEST_PATH,
                                'reference_attrs_multi_constraints.c')
        ref_base = open(path_base, "r")
        ref = open(path_ref, "r")
        self.assertEqual(ref.read(), ref_base.read())

    def test_attrs_invalid_1(self):
        self.run_iopc2('attrs_invalid_1.iop', False,
                       'incorrect attribute name')

    def test_attrs_invalid_2(self):
        self.run_iopc2('attrs_invalid_2.iop', False,
                       'attribute ctype does not apply to fields')

    def test_attrs_invalid_3(self):
        self.run_iopc2('attrs_invalid_3.iop', False,
                       'attribute ctype does not apply to interface')

    def test_attrs_invalid_4(self):
        self.run_iopc2('attrs_invalid_4.iop', False,
                       'attribute private does not apply to required fields')

    def test_attrs_invalid_5(self):
        self.run_iopc2('attrs_invalid_5.iop', False,
                       'attribute prefix does not apply to fields')

    def test_attrs_invalid_6(self):
        self.run_iopc2('attrs_invalid_6.iop', False,
                       'attribute minOccurs does not apply to fields with '  \
                       'default value')

    def test_attrs_invalid_7(self):
        self.run_iopc2('attrs_invalid_7.iop', False,
                       'attribute maxOccurs does not apply to required '     \
                       'fields')

    def test_attrs_invalid_8(self):
        self.run_iopc2('attrs_invalid_8.iop', False,
                       'unknown field c in MyUnion')

    def test_attrs_invalid_9(self):
        self.run_iopc2('attrs_invalid_9.iop', False,
                       'unknown field c in MyUnion')

    def test_attrs_invalid_10(self):
        self.run_iopc2('attrs_invalid_10.iop', False,
                       'cannot use both @allow and @disallow '               \
                       'on the same field')

    def test_attrs_invalid_11(self):
        self.run_iopc2('attrs_invalid_11.iop', False,
                       'unknown field c in MyUnion')

    def test_attrs_invalid_12(self):
        self.run_iopc2('attrs_invalid_12.iop', False,
                       'unknown field c in MyUnion')

    def test_attrs_invalid_13(self):
        self.run_iopc2('attrs_invalid_13.iop', False,
                       'unknown field C in MyEnum')

    def test_attrs_invalid_14(self):
        self.run_iopc2('attrs_invalid_14.iop', False,
                       'unknown field C in MyEnum')

    def test_attrs_invalid_15(self):
        self.run_iopc2('attrs_invalid_15.iop', False,
                       'invalid default value on enum field: 1')

    def test_attrs_invalid_16(self):
        self.run_iopc2('attrs_invalid_16.iop', False,
                       'invalid default value on enum field: 0')

    def test_attrs_invalid_17(self):
        f = 'attrs_invalid_17.iop'
        self.run_iopc(f, False,
                      'all static attributes must be declared first', 4)
        self.run_iopc(f, True,
                      'all static attributes must be declared first', 2)

    def test_attrs_invalid_18(self):
        f = 'attrs_invalid_18.iop'
        self.run_iopc(f, False,
                      'error: invalid ctype user_t: missing __t suffix', 4)

    def test_attrs_invalid_enumval(self):
        self.run_iopc2('attrs_invalid_enumval.iop', False,
                       'invalid attribute min on enum field')

    def test_tag_static_invalid(self):
        self.run_iopc2('tag_static_invalid.iop', False,
                       'tag is not authorized for static class fields')

    def test_default_char_valid(self):
        self.run_iopc_pass('pkg_d.iop', 1)

    def test_prefix_enum(self):
        self.run_iopc_pass('enum1.iop', 2)
        f = 'enum2.iop'
        self.run_iopc2(f, True, None)
        self.run_gcc(f)

    def test_dup_enum(self):
        self.run_iopc('enum3.iop', False,
                      'enum field name `A` is used twice', 2)
        self.run_iopc('enum4.iop', False,
                      'enum field name `A` is used twice', 2)

    def test_ambiguous_enum(self):
        self.run_iopc_pass('enum1.iop', 2)
        f = 'enum5.iop'
        self.run_iopc(f, True,
                      'enum field identifier `MY_ENUM_A` is ambiguous', 2)
        self.run_gcc(f)

    def test_deprecated_enum(self):
        self.run_iopc2('enum6.iop', True, None)

    def test_invalid_comma_enum(self):
        self.run_iopc('enum_invalid_comma.iop', False,
                      '`,` expected on every line', 2)

    def test_struct_noreorder(self):
        f = 'struct_noreorder.iop'
        self.run_iopc2(f, True, None)
        self.run_gcc(f)

    def test_snmp_struct_obj(self):
        f = 'snmp_obj.iop'
        self.run_iopc_pass(f, 4)
        self.run_gcc(f)

    def test_snmp_struct_obj_2(self):
        f = 'snmp_obj2.iop'
        self.run_iopc_pass(f, 4)
        self.run_gcc(f)

    def test_snmp_valid_iface(self):
        f = 'snmp_valid_iface.iop'
        self.run_iopc_pass(f, 4)
        self.run_gcc(f)

    def test_snmp_valid_iface_params_from_diff_pkg(self):
        f = 'snmp_valid_params_from_different_pkg.iop'
        self.run_iopc_pass(f, 4)
        self.run_gcc(f)

    def test_snmp_invalid_use_out_throw_snmp_iface(self):
        self.run_iopc('snmp_invalid_iface1.iop', False,
                      'snmpIface cannot out and/or throw', 4)

    def test_snmp_invalid_params_from_snmp_iface(self):
        self.run_iopc('snmp_invalid_iface2.iop', False,
                      'pkg `snmp_invalid_iface2` does not provide snmpObj '  \
                      '`IncorrectParams` when resolving snmp params of '     \
                      'snmpIface `Notifications`', 4)

    def test_snmp_invalid_several_identic_field_snmp_iface(self):
        self.run_iopc('snmp_invalid_iface3.iop', False,
                      'several snmpObjs given by the attribute '             \
                      'snmpParamsFrom have a field with the same name `c`', 4)

    def test_snmp_invalid_type_fields(self):
        self.run_iopc('snmp_invalid_fields.iop', False,
                      'only int/string/boolean/enum types are handled for '
                      'snmp objects\' fields', 4)

    def test_snmp_invalid_brief_field(self):
        self.run_iopc('snmp_invalid_brief_field.iop', False,
                      'field `a` needs a brief that would be used as a '
                      'description in the generated MIB', 4)

    def test_snmp_invalid_brief_rpc(self):
        self.run_iopc('snmp_invalid_brief_rpc.iop', False,
                      'notification `notif` needs a brief that would be used '
                      'as a description in the generated MIB', 4)

    def test_snmp_invalid_struct_type_for_field(self):
        self.run_iopc('snmp_invalid1.iop', False,
                      'only int/string/boolean/enum types are handled for '
                      'snmp objects\' fields', 4)

    def test_snmp_invalid_snmp_obj_type_for_field(self):
        self.run_iopc('snmp_invalid2.iop', False,
                      'snmp objects cannot be used to define a field type', 4)

    def test_snmp_valid_tbl(self):
        f = 'snmp_tbl.iop'
        self.run_iopc_pass(f, 4)
        self.run_gcc(f)

    def test_snmp_valid_enum(self):
        f = 'snmp_valid_enum.iop'
        self.run_iopc_pass(f, 4)
        self.run_gcc(f)

    def test_attrs_invalid_noreorder(self):
        self.run_iopc2('attrs_invalid_noreorder.iop', False,
                       'attribute noReorder does not apply to union')

    def test_integer_ext_valid(self):
        b = 'integer_ext_valid'
        self.run_iopc_pass(b + '.iop', 2)
        self.run_gcc(b + '.iop')
        self.check_file(b + '-tdef.iop.h', [
            'VAL_A = 123,', 'VAL_B = 83,', 'VAL_C = 6575,', 'VAL_D = 102400,',
            'VAL_E = 10240,', 'VAL_F = 10485760,', 'VAL_G = -10240,',
            'VAL_H = -2048,', 'VAL_I = 255,', 'VAL_O = 0,'])
        self.check_file(b + '.iop.c', [
            '{ .defval_u64 = 0x8000000000000000 }',
            '{ .defval_u64 = 0xffffffffffffffff }',
            '{ .defval_data = "RST" }'])

    def test_defval(self):
        tests_invalid = [
            {'f' : 'defval_bool_invalid.iop',
             's' : 'invalid default value on bool field'},
            {'f' : 'defval_double_nonzero.iop',
             's' : 'violation of @nonZero constraint'},
            {'f' : 'defval_double_string.iop',
             's' : 'string default value on double field'},
            {'f' : 'defval_enum_strict.iop',
             's' : 'invalid default value on strict enum field'},
            {'f' : 'defval_int_invalid.iop',
             's' : 'invalid default value on integer field'},
            {'f' : 'defval_int_max.iop',
             's' : 'violation of @max constraint'},
            {'f' : 'defval_int_min.iop',
             's' : 'violation of @min constraint'},
            {'f' : 'defval_int_nonzero.iop',
             's' : 'violation of @nonZero constraint'},
            {'f' : 'defval_int_unsigned.iop',
             's' : 'invalid default value on unsigned integer field'},
            {'f' : 'defval_str_invalid.iop',
             's' : 'invalid default value on string field'},
            {'f' : 'defval_str_maxlength.iop',
             's' : 'violation of @maxLength constraint'},
        ]
        for t in tests_invalid:
            self.run_iopc(t['f'], False, t['s'], 2)

        f = 'defval_valid.iop'
        self.run_iopc2(f, True, None)
        self.run_gcc(f)

    def test_integer_ext_overflow(self):
        self.run_iopc('integer_ext_overflow.iop', False,
                      'integer overflow', 2)

    def test_integer_ext_invalid(self):
        self.run_iopc('integer_ext_invalid.iop', False,
                      'invalid integer extension', 2)

    def test_same_as(self):
        self.run_iopc_pass('same_as.iop', 4)
        self.run_gcc('same_as.iop')
        self.check_file('same_as.iop.c', wanted = True, string_list = [
            'same_as__struct1__desc_fields', 'same as same_as.Struct1',
            'same_as__union1__desc_fields',  'same as same_as.Union1',
            'same_as__class1__desc_fields',  'same as same_as.Class1',
            'same_as__interface1__bar_args__desc_fields',
            'same_as__interface1__bar_res__desc_fields',
            'same_as__interface1__bar_exn__desc_fields',
            'same as same_as.Interface1.barArgs',
            'same as same_as.Interface1.barRes',
            'same as same_as.Interface1.barExn'])
        self.check_file('same_as.iop.c', wanted = False, string_list = [
            'same_as__struct2__desc_fields',
            'same as same_as.Struct3',
            'same as same_as.Struct4',
            'same_as__union2__desc_fields',
            'same_as__class2__desc_fields',
            'same_as__interface1__foo_args__desc_fields',
            'same_as__interface1__foo_res__desc_fields',
            'same_as__interface1__foo_exn__desc_fields',
            'same_as__interface2__rpc_args__desc_fields',
            'same_as__interface2__rpc_res__desc_fields',
            'same_as__interface2__rpc_exn__desc_fields'])

    # {{{ Inheritance

    def test_inheritance_invalid_multiple_inheritance(self):
        self.run_iopc2('inheritance_invalid_multiple_inheritance.iop', False,
                       'multiple inheritance is not supported')

    def test_inheritance_invalid_types(self):
        self.run_iopc2('inheritance_invalid_types1.iop', False,
                       '`{` expected, but got `:`')
        self.run_iopc2('inheritance_invalid_types2.iop', False,
                       '`{` expected, but got `:`')
        self.run_iopc2('inheritance_invalid_types3.iop', False,
                       'parent object `Father` is not a class')
        self.run_iopc_fail('inheritance_invalid_types4.iop',
                           'only classes can be abstrac')

    def test_inheritance_invalid_circular1(self):
        self.run_iopc2('inheritance_invalid_circular1.iop', False,
                       ['circular dependency',
                        'inheritance_invalid_circular1.iop:3:2:  '           \
                        'from: class A',
                        'class A inherits from class C2',
                        'class C2 inherits from class B',
                        'class B inherits from class A'])

    def test_inheritance_invalid_circular2(self):
        self.run_iopc2('inheritance_invalid_circular2_pkg_a.iop', False,
                       'circular dependency')
        self.run_iopc2('inheritance_invalid_circular2_pkg_b.iop', False,
                       'circular dependency')

    def test_inheritance_invalid_duplicated_fields(self):
        self.run_iopc2('inheritance_invalid_duplicated_fields.iop', False,
                       'field name `a` is also used in child `C`')

    def test_inheritance_invalid_ids(self):
        self.run_iopc2('inheritance_invalid_id_small.iop', False,
                       'id is too small (must be >= 0, got -1)')
        self.run_iopc2('inheritance_invalid_id_large.iop', False,
                       'id is too large (must be <= 65535, got 65536)')
        self.run_iopc2('inheritance_invalid_id_missing.iop', False,
                       'integer expected, but got identifier instead')
        self.run_iopc2('inheritance_invalid_id_duplicated.iop', False,
                       '26:10: error: id 3 is also used by class `C1`')

    def test_inheritance_invalid_static(self):
        self.run_iopc('inheritance_invalid_static_struct.iop', False,
                      'static keyword is only authorized for class fields')
        self.run_iopc2('inheritance_invalid_static_no_default.iop', False,
                       'static fields of non-abstract classes must '         \
                       'have a default value')
        self.run_iopc2('inheritance_invalid_static_repeated.iop', False,
                       'repeated static members are forbidden')
        self.run_iopc2('inheritance_invalid_static_optional.iop', False,
                       'optional static members are forbidden')
        self.run_iopc2('inheritance_invalid_static_type_mismatch.iop', False,
                       'incompatible type for static field `toto`: '         \
                       'should be `int`')
        self.run_iopc2('inheritance_invalid_static_already_defined.iop',
                       False, 'field `toto` already defined by parent `A`')
        self.run_iopc2('inheritance_invalid_static_abstract_not_defined.iop',
                       False, 'error: class `D1` must define a static '      \
                       'field named `withoutDefval`')

    def test_inheritance_class_id_ranges(self):
        self.run_iopc_fail('', 'invalid class-id-range',
                           class_id_range = '10')
        self.run_iopc_fail('', 'invalid class-id-range',
                           class_id_range = '-10')
        self.run_iopc_fail('', 'invalid class-id-range',
                           class_id_range = '-10-10')
        self.run_iopc_fail('', 'invalid class-id-range',
                           class_id_range = '65536-10')
        self.run_iopc_fail('', 'invalid class-id-range',
                           class_id_range = '10-9')
        self.run_iopc_fail('', 'invalid class-id-range',
                           class_id_range = '10-65536')

        self.run_iopc_pass('inheritance_pkg_b.iop', 3, class_id_range = '')
        self.run_iopc_pass('inheritance_pkg_b.iop', 3,
                           class_id_range = '10-19')
        self.run_iopc('inheritance_pkg_b.iop', False,
                      'id is too small (must be >= 11, got 10)', 3,
                      class_id_range = '11-19')
        self.run_iopc('inheritance_pkg_b.iop', False,
                      'id is too large (must be <= 18, got 19)', 3,
                      class_id_range = '10-18')

    def test_inheritance(self):
        self.run_iopc_pass('inheritance_pkg_a.iop', 3)
        self.run_iopc_pass('inheritance_pkg_b.iop', 3)
        self.run_iopc_pass('inheritance_valid_local_pkg.iop', 3)
        self.run_iopc_pass('inheritance_pkg_a.iop', 4)
        self.run_iopc_pass('inheritance_pkg_b.iop', 4)
        self.run_iopc_pass('inheritance_valid_local_pkg.iop', 4)
        self.run_gcc('inheritance_pkg_a.iop')
        self.run_gcc('inheritance_pkg_b.iop')

        # Check that ClassContainer fields (which are classes) are pointed
        self.check_file('inheritance_pkg_a-t.iop.h', string_list = [
            'struct inheritance_pkg_a__a1__t *class_container_a1;',
            'struct inheritance_pkg_a__b1__t *class_container_b1;',
            'inheritance_pkg_a__a2__array_t class_container_a2;'])

        # Check the "same as" feature with inheritance
        self.check_file('inheritance_pkg_b.iop.c', wanted = True,
                        string_list = [
                            'inheritance_pkg_b__a1__desc_fields',
                            'inheritance_pkg_b__a2__desc_fields',
                            'inheritance_pkg_b__a3__desc_fields',
                            'inheritance_pkg_b__a4__desc_fields',
                            'inheritance_pkg_b__a5__desc_fields',
                            'inheritance_pkg_b__a6__class_s',
                            'inheritance_pkg_b__a7__desc_fields',
                            'inheritance_pkg_b__a9__desc_fields',
                            'inheritance_pkg_b__a10__desc_fields',
                            'same as inheritance_pkg_b.A5',
                            'same as inheritance_pkg_b.A7'])
        self.check_file('inheritance_pkg_b.iop.c', wanted = False,
                        string_list = [
                            'same as inheritance_pkg_b.A1',
                            'same as inheritance_pkg_b.A3',
                            'same as inheritance_pkg_b.A9',
                            'inheritance_pkg_b__a6__desc_fields',
                            'inheritance_pkg_b__a8__desc_fields'])

    def test_inheritance_invalid_local_pkg(self):
        self.run_iopc('inheritance_invalid_local_pkg.iop', False,
                      'as the parent class `Local1` of class `NonLocal`'
                      ' is `local`, both classes need to be'
                      ' in the same package', 3)

    # }}}
    # {{{ Typedef

    def test_typedef_valid(self):
        f  = 'typedef_valid_no_class.iop'
        f1 = 'typedef1.iop'
        f2 = 'typedef2.iop'
        self.run_iopc2(f, True, None)
        self.run_iopc(f1, True, None)
        self.run_iopc(f2, True, None)
        self.run_gcc(f)
        self.run_gcc(f1)
        self.run_gcc(f2)
        self.run_iopc('typedef_valid.iop', False,
                      'type `MyType` is provided by both `typedef2` '        \
                      'and `typedef1`', 3)
        self.run_iopc('typedef_valid.iop', False,
                      'type `MyType` is provided by both `typedef2` '        \
                      'and `typedef1`', 4)

    def test_typedef_invalid(self):
        self.run_iopc('typedef_invalid_1.iop', False,
                      'unable to find any pkg providing type `MyStruct`')
        self.run_iopc2('typedef_invalid_2.iop', False,
                       'attribute ctype does not apply to typedefs')
        self.run_iopc2('typedef_invalid_3.iop', False,
                       'attribute pattern does not apply to integer')
        self.run_iopc('typedef_invalid_6.iop', False,
                      'recursive typedef for type `MyTypedef1` in pkg '      \
                      '`typedef_invalid_6`')
        self.run_iopc('typedef_invalid_7.iop', False,
                      'cannot declare repeated optional fields')
        self.run_iopc('typedef_invalid_8.iop', False,
                      'optional members are forbidden in union types')
        self.run_iopc('typedef_invalid_9.iop', False,
                      'cannot declare repeated optional fields')
        self.run_iopc('typedef_invalid_10.iop', False,
                      'cannot declare repeated optional fields')
        self.run_iopc('typedef_invalid_11.iop', False,
                      'repeated members are forbidden in union types')
        self.run_iopc2('typedef_invalid_12.iop', False,
                       'pkg `typedef_invalid_12` does not provide class '    \
                       '`B` when resolving inheritance of class `C`')
        self.run_iopc2('typedef_invalid_13.iop', False,
                       'attribute minOccurs does not apply to required '     \
                       'typedefs')

    # }}}
    # {{{ Generic attributes

    def test_generic_invalid_value(self):
        self.run_iopc2('generic_attrs_invalid_1.iop', False,
                       'unable to parse value for generic argument '         \
                       '\'test:gen1\'')

    def test_generic_invalid_key(self):
        self.run_iopc2('generic_attrs_invalid_2.iop', False,
                       'generic attribute name (namespace:id) expected, '    \
                       'but got identifier instead')

    def test_generic_repeated_key(self):
        self.run_iopc2('generic_attrs_invalid_3.iop', False,
                       'generic attribute \'test:gen3\' must be unique for ' \
                       'each IOP object')

    def test_generic_invalid_len(self):
        self.run_iopc2('generic_attrs_invalid_4.iop', False,
                       'error: `)` expected, but got `,` instead')

    def test_generic_json(self):
        f = 'json_generic_attributes'
        g = os.path.join(TEST_PATH, f)
        self.run_iopc_pass(f + '.iop', 3, 'C,json')
        self.run_iopc_pass(f + '.iop', 4, 'C,json')
        self.run_gcc(f + '.iop')
        for lang in ['json', 'c']:
            self.check_ref(g, lang)
        self.run_iopc2('json_generic_invalid1.iop', False,
                       'error: `:` expected, but got `)` instead')
        self.run_iopc2('json_generic_invalid2.iop', False,
                       'error: invalid token when parsing json value')
        self.run_iopc2('json_generic_invalid3.iop', False,
                       'error: string expected, but got `,` instead')
        self.run_iopc2('json_generic_invalid4.iop', False,
                       'error: `]` expected, but got integer instead')
        self.run_iopc2('json_generic_invalid5.iop', False,
                       'error: `)` expected, but got identifier instead')

    # }}}
    # {{{ References

    def test_reference_valid(self):
        f  = "reference.iop"
        self.run_iopc(f, True, None)
        self.run_gcc(f)

    def test_reference_invalid(self):
        self.run_iopc('reference_invalid_1.iop', False,
                      'references can only be applied to structures or '     \
                      'unions')
        self.run_iopc('reference_invalid_2.iop', False,
                      'references can only be applied to structures or '     \
                      'unions')
        self.run_iopc2('reference_invalid_3.iop', False,
                       'references can only be applied to structures or '    \
                       'unions')
        self.run_iopc('reference_invalid_4.iop', False,
                      'references can only be applied to structures or '     \
                      'unions')
        self.run_iopc('reference_invalid_5.iop', False,
                      'circular dependency')
        self.run_iopc('reference_invalid_6.iop', False,
                      'identifier expected, but got `?` instead')
        self.run_iopc('reference_invalid_7.iop', False,
                      'identifier expected, but got `&` instead')
        self.run_iopc2('reference_invalid_8.iop', False,
                       'referenced static members are forbidden')
        self.run_iopc('reference_invalid_9.iop', False,
                      'circular dependency')

    # }}}

    def test_json(self):
        f = 'tstjson'
        g = os.path.join(TEST_PATH, f)
        for lang in ['json', 'C,json', 'json,C']:
            subprocess.call(['rm', '-f', g + '.iop.json'])
            self.run_iopc_pass(f + '.iop', 3, lang)
            self.check_ref(g, 'json')

    def test_dox_c(self):
        f = 'tstdox'
        g = os.path.join(TEST_PATH, f)
        subprocess.call(['rm', '-f', g + '.iop.c'])
        self.run_iopc_pass(f + '.iop', 3)
        self.run_iopc_pass(f + '.iop', 4)
        self.run_gcc(f + '.iop')
        self.check_ref(g, 'c')

    def test_gen_c(self):
        f = 'tstgen'
        g = os.path.join(TEST_PATH, f)
        subprocess.call(['rm', '-f', g + '.iop.c'])
        self.run_iopc_pass(f + '.iop', 3)
        self.run_iopc_pass(f + '.iop', 4)
        self.run_gcc(f + '.iop')
        self.check_ref(g, 'c')
        self.check_ref(g + '-t', 'h')

if __name__ == "__main__":
    z.main()
