--TEST--
Add immutable param modifier
--FILE--
<?php
class One {
	public immutable $number = 0;
	public function __construct(int $number) {
		$this->number = $number;
	}
}
$one = new One(15);
$property = (new ReflectionClass(One::class))->getProperty('number');
var_dump($property->isImmutable());
var_dump($property->isStatic());

var_dump($one->number);

$one->number = 150; // fail
var_dump($one->number);
--EXPECTF--
bool(true)
bool(false)
int(15)

Warning: Cannot change immutable property: One::$number in %s on line 13
int(15)
