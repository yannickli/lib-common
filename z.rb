#!/usr/bin/env ruby
# encoding: utf-8
#
# = z.rb
#
# Copyright (C) 2004-2011 INTERSEC SAS
#
# Should you receive a copy of this source code, you must check you have
# a proper, written authorization of INTERSEC to hold it. If you don't have
# such an authorization, you must DELETE all source code files in your
# possession, and inform INTERSEC of the fact you obtain these files. Should
# you not comply to these terms, you can be prosecuted in the extent permitted
# by applicable law.
#
#
# == module Z
#
# z is a module extending "test/unit" to support the Z protocol and to add
# several helpers. The "test/unit" gem version >= 2 is required.
# (http://test-unit.rubyforge.org/test-unit/en/Test/Unit.html)
#
# Just include this file and use Test::Unit as usual. Your TestSuite will be
# launched automatically. If Z_HARNESS is set, then we will run in "Z"
# compatibilty mode.
#
#
# === Zflags
#
# The Z module also provides a <tt>zflag</tt> method which allows to set
# one or several flags for a test. If one of these flags are contained in the
# <tt>Z_TAG_SKIP</ttl> environment variable the test will be skipped.
#
#
# === Zmodes
#
# The Z module provides a <tt>z_has_mode?</tt> method which allows to set if
# a given Z_MODE is enabled.
#

require 'rubygems'
gem 'test-unit'
require 'test/unit'
require 'test/unit/ui/testrunner'
require 'test/unit/ui/testrunnermediator'

def zdebug(obj)
  puts "########"
  puts obj.inspect
  puts "########"
end

module Test
  module Unit
    module UI
      module Z
        # Implements Z protocol in a Test::Unit UI runner
        class TestRunner < Test::Unit::UI::TestRunner
          def initialize(suite, options={})
            super
            @output = @options[:output] || STDOUT
            @n_tests = 0
            @already_outputted = false
            @current_test = nil
          end

          private
          def attach_to_mediator
            @mediator.add_listener(TestResult::FAULT,
                                   &method(:add_fault))
            @mediator.add_listener(TestRunnerMediator::STARTED,
                                   &method(:started))
            @mediator.add_listener(TestRunnerMediator::FINISHED,
                                   &method(:finished))
            @mediator.add_listener(TestCase::STARTED_OBJECT,
                                   &method(:test_started))
            @mediator.add_listener(TestCase::FINISHED_OBJECT,
                                   &method(:test_finished))
          end

          def add_fault(fault)
            what = "fail"
            dump_long = true

            case fault.single_character_display
            when 'P'
              if fault.instance_variable_get(:@zpassed)
                what = "todo-pass"
              else
                what = "todo-fail"
                dump_long = false
              end
            when 'O'
              what = "skip"
              dump_long = false
            end

            if dump_long
              ccn = @current_test.class.name
              cmn = @current_test.method_name
              puts("#{@n_tests} #{what} #{fault.test_name}")
              puts(": #{$0} -t #{ccn} -n #{cmn}", ':')
              fault.long_display.each_line do |line|
                puts(": #{line}")
              end
            else
              puts("#{@n_tests} #{what} #{fault.test_name}" +
                   " # #{fault.message.split("\n")[0]}")
            end
            @already_outputted = true
          end

          def started(result)
            @result = result
            puts("1..#{@suite.size} #{@suite.name}")
          end

          def finished(elapsed_time)
            puts("# Finished in #{elapsed_time} seconds.")
            @result.to_s.each_line do |line|
              puts("# #{line}")
            end
          end

          def test_started(test)
            @n_tests += 1
            @current_test = test
          end

          def test_finished(test)
            puts("#{@n_tests} pass #{test}") unless @already_outputted
            @already_outputted = false
            @current_test = nil
          end

          def puts(*args)
            @output.puts(*args)
            @output.flush
          end
        end
      end
    end

    module TestCasePendingSupport
      # We hack the pend function because we do not support the usage of
      # pend() without a block
      def pend(message=nil, &block)
        message ||= "pended."
        if block_given?
          pending = nil
          begin
            yield
          rescue Exception
            pending = Pending.new(name, filter_backtrace(caller), message)
            pending.instance_variable_set(:@zpassed, false)
            add_pending(pending)
          end
          unless pending
            pending = Pending.new(name, filter_backtrace(caller), message)
            pending.instance_variable_set(:@zpassed, true)
            add_pending(pending)
          end
        else
          flunk("pend() usage without a block isn't supported in Z module!")
        end
      end
    end

    $z_modes = ENV['Z_MODE'].to_s.split
    $z_flags = ENV['Z_TAG_SKIP'].to_s.split

    class TestCase
      # Returns wether a Z mode is enabled
      def z_has_mode?(mode)
        $z_modes.include?(mode.to_s)
      end

      private
      def zflags(*flags)
        flags.each do |flag|
          if $z_flags.include?(flag.to_s)
            omit("skipping tests flagged with #{flag}")
          end
        end
      end
    end

    # register our Z runner
    AutoRunner.register_runner(:z) do |auto_runner|
      Test::Unit::UI::Z::TestRunner
    end
  end
end

Test::Unit::AutoRunner.default_runner = "z" if ENV['Z_HARNESS']
