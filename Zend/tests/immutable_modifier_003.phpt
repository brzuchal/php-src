--TEST--
Modify stae of immutable object after constructor.
--FILE--
<?php

immutable class Foo {
	public $bar;
	public function __construct($bar) {
		$this->bar = $bar;
	}
}

$foo = new Foo(1);
$foo->bar = 2;

?>
--EXPECTF--

Fatal error: Uncaught Error: Can not modify state of immutable object after constructor in %simmutable_modifier_003.php:11
Stack trace:
#0 {main}
  thrown in %simmutable_modifier_003.php on line 11