--TEST--
collection values: malformed / hostile unserialize input fails cleanly
--FILE--
<?php

/* unserialize() input is untrusted. Every malformed, unsupported or
 * type-violating collection payload must return false (with a notice), never
 * crash, misparse, or mint a value the language could not construct. */

function u(string $s) {
    $r = @unserialize($s);
    return $r === false ? 'false' : get_debug_type($r);
}

$bad = [
    'unknown kind name'      => 'L:nope:1:{i;}:1:{i:1;}',
    'numeric kind (frozen?)' => 'L:0:1:{i;}:1:{i:1;}',
    'unknown member tag'     => 'L:vec:1:{z;}:1:{i:1;}',
    'element type mismatch'  => 'L:vec:1:{i;}:1:{s:1:"x";}',
    'not-constructible kind' => 'L:map:2:{i;s;}:1:{i:1;}',
    'set member nested'      => 'L:set:1:{l:vec:1:{i;};}:1:{L:vec:1:{i;}:1:{i:1;}}',
    'wrong element count'    => 'L:vec:1:{i;}:3:{i:1;i:2;}',
    'too many elements'      => 'L:tuple:2:{i;s;}:3:{i:1;s:1:"a";i:2;}',
    'arity zero'             => 'L:vec:0:{}:0:{}',
    'huge count'             => 'L:vec:1:{i;}:99999999:{i:1;}',
    'truncated descriptor'   => 'L:vec:1:{i;',
    'truncated elements'     => 'L:vec:1:{i;}:2:{i:1;',
    'missing class'          => 'L:vec:1:{c:9:"NoSuchCls";}:1:{i:1;}',
    'class elem type wrong'  => 'L:vec:1:{c:8:"stdClass";}:1:{i:1;}',
    'nested wrong kind'      => 'L:vec:1:{l:vec:1:{i;};}:1:{L:tuple:1:{i;}:1:{i:1;}}',
];
foreach ($bad as $label => $s) {
    printf("%-24s %s\n", $label, u($s));
}

echo "-- a valid string still works next to the bad ones --\n";
var_dump(u('L:vec:1:{i;}:2:{i:1;i:2;}'));

echo "-- deep descriptor nesting is bounded, not a stack smash --\n";
$deep = str_repeat('L:vec:1:{', 200) . 'i;' . str_repeat('};', 200);
var_dump(u('L:vec:1:{' . $deep . '}:0:{}'));

?>
--EXPECT--
unknown kind name        false
numeric kind (frozen?)   false
unknown member tag       false
element type mismatch    false
not-constructible kind   false
set member nested        false
wrong element count      false
too many elements        false
arity zero               false
huge count               false
truncated descriptor     false
truncated elements       false
missing class            false
class elem type wrong    false
nested wrong kind        false
-- a valid string still works next to the bad ones --
string(10) "collection"
-- deep descriptor nesting is bounded, not a stack smash --
string(5) "false"
