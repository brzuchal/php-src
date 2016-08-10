--TEST--
Set state of the object in constructor.
--FILE--
<?php

immutable class Foo {
	public $bar;
	public function __construct($bar) {
		$this->bar = $bar;
		echo "Ok\n";
	}
}

$foo = new Foo(1);

?>
--EXPECT--

Ok