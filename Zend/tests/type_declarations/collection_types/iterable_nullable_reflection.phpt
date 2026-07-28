--TEST--
F2: Reflection for nullable iterable is unchanged -- ?iterable is a named type; iterable|null / null|iterable stay ReflectionUnionType("Traversable|array|null")
--FILE--
<?php
function show(string $label, ReflectionType $t): void {
    $builtin = $t instanceof ReflectionNamedType ? var_export($t->isBuiltin(), true) : 'union';
    $members = $t instanceof ReflectionUnionType
        ? '[' . implode(',', array_map(fn($m) => (string) $m, $t->getTypes())) . ']'
        : '-';
    printf("%s = %s / %s / builtin=%s / null=%s / getTypes=%s\n",
        $label, get_class($t), (string) $t, $builtin, var_export($t->allowsNull(), true), $members);
}
function a(?iterable $x) {}
function b(iterable|null $x) {}
function d(null|iterable $x) {}
function e(array|Traversable|null $x) {}
function g(iterable $x) {}

show('?iterable', (new ReflectionFunction('a'))->getParameters()[0]->getType());
show('iterable|null', (new ReflectionFunction('b'))->getParameters()[0]->getType());
show('null|iterable', (new ReflectionFunction('d'))->getParameters()[0]->getType());
show('array|Traversable|null', (new ReflectionFunction('e'))->getParameters()[0]->getType());
show('iterable', (new ReflectionFunction('g'))->getParameters()[0]->getType());
echo "ok\n";
?>
--EXPECT--
?iterable = ReflectionNamedType / ?iterable / builtin=true / null=true / getTypes=-
iterable|null = ReflectionUnionType / Traversable|array|null / builtin=union / null=true / getTypes=[Traversable,array,null]
null|iterable = ReflectionUnionType / Traversable|array|null / builtin=union / null=true / getTypes=[Traversable,array,null]
array|Traversable|null = ReflectionUnionType / Traversable|array|null / builtin=union / null=true / getTypes=[Traversable,array,null]
iterable = ReflectionNamedType / iterable / builtin=true / null=false / getTypes=-
ok
