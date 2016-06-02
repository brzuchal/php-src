--TEST--
ZE2 An package-private class instantiation and method call in (sub)package namespace and error from different package namespace
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
		function show() {
			$this->test->show();
		}
	}
    $packageTest = new PackageClass();
    $packageTest->create();
    $packageTest->show();
}
namespace Test\SubTest {
    class SubPackageClass {
        function create() {
            $this->test = new \Test\PackagePrivateClass();
        }
        function show() {
            $this->test->show();
        }
    }
    $subPackageTest = new SubPackageClass();
    $subPackageTest->create();
    $subPackageTest->show();
}
namespace ErrorTest {
    class ErrorPackageClass {
        function create() {
            $this->test = new \Test\PackagePrivateClass();
        }
        function show() {
            $this->test->show();
        }
    }
    $subPackageTest = new ErrorPackageClass();
    $subPackageTest->create();
    $subPackageTest->show();
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

