--TEST--
vec: withAt()/withoutAt() Reflection — int $index / mixed $value, vec[] return, receiver-isolated
--FILE--
<?php
$v = vec[int]{1, 2, 3};
foreach (['withAt', 'withoutAt'] as $name) {
    $f = $v->$name(...);
    $r = new ReflectionFunction($f);
    echo "$name: return=", (string)$r->getReturnType(),
         " required=", $r->getNumberOfRequiredParameters();
    foreach ($r->getParameters() as $p) {
        echo " | ", $p->getName(), ":", (string)$p->getType();
    }
    echo "\n";
    // receiver isolation: no bound $this, no leaked static variables
    var_dump($r->getClosureThis() === null);
    var_dump($r->getStaticVariables() === []);
}
?>
--EXPECT--
withAt: return=vec[] required=2 | index:int | value:mixed
bool(true)
bool(true)
withoutAt: return=vec[] required=1 | index:int
bool(true)
bool(true)
