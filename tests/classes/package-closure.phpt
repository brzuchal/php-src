--TEST--
ZE2 An package-private class instantiation and method call through Closure in (sub)package namespace and error from different package namespace
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
    $test = function () {
        return new PackagePrivateClass();
    };
    $test()->show();
}
namespace Test\SubTest {
    $subtest = function () {
        return new \Test\PackagePrivateClass();
    };
    $subtest()->show();
}
namespace ErrorTest {
    $errortest = function () {
        return new \Test\PackagePrivateClass();
    };
    $errortest()->show();
}
?>
--EXPECTF--
Call to function show() from package private class
Call to function show() from package private class

Fatal error: Uncaught Error: Cannot instantiate package class Test\PackagePrivateClass in ErrorTest\{closure} in %s:%d
Stack trace:
#0 %s(%d): ErrorTest\{closure}()
#1 {main}
  thrown in %s on line %d

