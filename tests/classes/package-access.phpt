--TEST--
ZE2 An package-private class object access in (sub)package namespace and error from different package namespace
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php
namespace Test {
	package class PackagePrivateClass {
		public $message = "Call to function show() from package private class\n";
		function show() {
			echo $this->message;
		}
	}
	class PackageClass extends PackagePrivateClass {
		function get() {
			return new PackagePrivateClass();
		}
	}
    $packageTest = new PackageClass();
	$packagePrivateObject = $packageTest->get();
	$packagePrivateObject->show();
	var_dump($packagePrivateObject->message);
}
namespace Test\SubTest {
    class SubPackageClass extends \Test\PackageClass {
    }
    $subPackageTest = new SubPackageClass();
	$packagePrivateObject = $subPackageTest->get();
	$packagePrivateObject->show();
	var_dump($packagePrivateObject->message);
}
namespace ErrorTest {
    class ErrorPackageClass {
		function get() {
			return new \Test\SubTest\SubPackageClass();
		}
    }
    $errorPackageTest = new ErrorPackageClass();
	$subPackageObject = $errorPackageTest->get();
	$packagePrivateObject = $subPackageObject->get();
	$packagePrivateObject->show();
	var_dump($packagePrivateObject->message);
}
?>
--EXPECTF--
Call to function show() from package private class

string(51) "Call to function show() from package private class
"
Call to function show() from package private class
string(51) "Call to function show() from package private class
"