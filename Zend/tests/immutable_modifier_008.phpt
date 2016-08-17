--TEST--
Assign instance of non-immutable class to immutable property.
--FILE--
<?php

class Bar{}

immutable class Foo {
	public $bar;

	public function __construct(Bar $bar) {
		$this->bar = $bar;
	}
}

$foo = new Foo(new Bar());
?>
--EXPECTF--

Fatal error: Uncaught Error: Immutable property must hold instance of immutable class in %simmutable_modifier_008.php:8
Stack trace:
#0 /tmp/cls.php(12): Foo->__construct(Object(Bar))
#1 {main}
  thrown in %simmutable_modifier_008.php on line 8
