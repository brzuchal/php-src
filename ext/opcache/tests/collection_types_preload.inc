<?php
function preloaded_f(vec[int] $a): ?vec[vec[string]] { return null; }
class PreloadedC {
    public vec[vec[int]] $deep;
    public function m(?vec[string] $s): vec[int] {}
}
