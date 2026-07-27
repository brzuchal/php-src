--TEST--
Bare collection-kind types: parse-error boundaries (map[]/shape[]/literal/adjacency/comment)
--DESCRIPTION--
map[]/shape[] are not runtime-ready (no grammar alternative); vec[]{} is a literal
(untouched); `vec []` and `vec/* */[` break head-'[' adjacency so are not a
collection head. All are parse errors, confirming the BC-free empty-descriptor
spelling does not leak into concrete literals or non-adjacent forms.
--FILE--
<?php
$cases = [
    'map[]'                => 'function u(map[] $x){}',
    'shape[]'              => 'function u(shape[] $x){}',
    'vec[]{} literal'      => '$y = vec[]{};',
    'vec [] (space)'       => 'function u(vec [] $x){}',
    'vec/* */[] (comment)' => 'function u(vec/* */[] $x){}',
];
foreach ($cases as $label => $code) {
    try { eval($code); echo "$label: NO ERROR\n"; }
    catch (\ParseError $e) { echo "$label: ParseError\n"; }
}
?>
--EXPECT--
map[]: ParseError
shape[]: ParseError
vec[]{} literal: ParseError
vec [] (space): ParseError
vec/* */[] (comment): ParseError
