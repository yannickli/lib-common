#!/usr/bin/env ruby
# encoding: utf-8
#
# = ztst-z.rb
#
# Copyright (C) INTERSEC SA           
#
# Should you receive a copy of this source code, you must check you have
# a proper, written authorization of INTERSEC to hold it. If you don't have
# such an authorization, you must DELETE all source code files in your
# possession, and inform INTERSEC of the fact you obtain these files. Should
# you not comply to these terms, you can be prosecuted in the extent permitted
# by applicable law.
#
# == Testing of the Z wrapper of Test::Unit
#

load 'z.rb'

class TestUnitZRunner < Test::Unit::TestCase
  def test_a_success
    assert_true(true, "true is true!")
  end

  def test_b_failure
    assert_false(true, "true is not false! :o")
  end

  def test_c_omit
    omit("Buggy test. I was so sure that true was false!")
    assert_false(true, "true is not false! :o")
  end

  def test_d_pending
    pend("One day I will complete this test (or not).") do
      assert_false(true, "true is not false! :o")
    end
  end

  def test_e_flag1
    zflags :slow, :foo
    assert_true(true, "it seems that true is always true!")
  end
end
