--TEST--
Assign by reference to immutable property.
--FILE--
<?php

immutable class Foo {
	public $bar;

	public function __construct($bar) {
		$this->bar = $bar;
	}
}

$foo = new Foo(1);
$bar = & $foo->bar;

$bar = 2;
?>
--EXPECTF--

Fatal error: Uncaught Error: Can not assign by reference to immutable property in %simmutable_modifier_007.php:14
Stack trace:
#0 {main}
  thrown in %simmutable_modifier_007.php on line 14