--TEST--
ZE2 An package-private class inheritance in (sub)package namespace and error from different package namespace
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php
namespace Test {
	package class PackagePrivateClass {
	}
	class PackageClass extends PackagePrivateClass {
	}
    $packageTest = new PackageClass();
}
namespace Test\SubTest {
    class SubPackageClass extends \Test\PackagePrivateClass {
    }
    $subPackageTest = new SubPackageClass();
}
namespace ErrorTest {
    class ErrorPackageClass extends \Test\PackagePrivateCLass {
    }
    $subPackageTest = new ErrorPackageClass();
}
?>
--EXPECTF--
Fatal error: Uncaught Error: Cannot instantiate package class Test\PackagePrivateClass in class ErrorTest\ErrorPackageClass in %s:%d
Stack trace:
#0 %s(%d): ErrorTest\ErrorPackageClass->create()
#1 {main}
  thrown in %s on line %d

