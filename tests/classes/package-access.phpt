--TEST--
ZE2 An package-private class access from non (sub)package namespace
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php
namespace Test {
	package class PackagePrivateClass {
		function show() {
			echo "Call to function show() from package private class\n";
		}
	}
	class PackageClass {
		function create() {
			$this->test = new PackagePrivateClass();
		}
	}
    $packageTest = new PackageClass();
    $packageTest->create();
    $packageTest->test->show();
}
namespace Test\SubTest {
    class SubPackageClass {
        function create() {
            $this->test = new \Test\PackagePrivateClass();
        }
    }
    $subPackageTest = new SubPackageClass();
    $subPackageTest->create();
    $subPackageTest->test->show();
}
namespace ErrorTest {
    class ErrorPackageClass {
        function create() {
            $this->subtest = new \Test\SubTest\SubPackageClass();
            $this->subtest->create();
        }
    }
    $subPackageTest = new ErrorPackageClass();
    $subPackageTest->create();
    $subPackageTest->subtest->test->show();
}
?>
--EXPECTF--
Call to function show() from package private class
Call to function show() from package private class

Fatal error: Uncaught Error: Cannot instantiate package class Test\PackagePrivateClass in class ErrorTest\ErrorPackageClass in %s:%d
Stack trace:
#0 %s(%d): ErrorTest\ErrorPackageClass->create()
#1 {main}
  thrown in %s on line %d

