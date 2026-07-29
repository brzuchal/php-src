--TEST--
vec: append()/prepend() Reflection — mixed $value, vec[] return, receiver-isolated
--FILE--
<?php
$v = vec[int]{1, 2};
foreach (['append', 'prepend'] as $name) {
    $f = $v->$name(...);
    $r = new ReflectionFunction($f);
    echo "$name: return=", (string)$r->getReturnType(),
         " required=", $r->getNumberOfRequiredParameters(),
         " param0=", $r->getParameters()[0]->getName(),
         ":", (string)$r->getParameters()[0]->getType(), "\n";
    // receiver isolation: no bound $this, no leaked static variables
    var_dump($r->getClosureThis() === null);
    var_dump($r->getStaticVariables() === []);
}
?>
--EXPECT--
append: return=vec[] required=1 param0=value:mixed
bool(true)
bool(true)
prepend: return=vec[] required=1 param0=value:mixed
bool(true)
bool(true)
