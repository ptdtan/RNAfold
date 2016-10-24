%{
/** @file version.i.in
 * @brief Set $RNA::VERSION to the bindings version
 */
%}

%perlcode %{
our $VERSION = '2.2.10';
sub VERSION () { $VERSION };
%}

