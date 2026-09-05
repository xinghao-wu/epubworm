function __fish_epubworm_needs_command
    test (count (commandline -opc)) -eq 1
end

function __fish_epubworm_using_command
    set -l tokens (commandline -opc)
    test (count $tokens) -ge 2; and test "$tokens[2]" = "$argv[1]"
end

complete -c epubworm -f

complete -c epubworm -n '__fish_epubworm_needs_command' -s h -l help -d 'Show help page'
complete -c epubworm -n '__fish_epubworm_needs_command' -a add -d 'Add .epub files to the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a remove -d 'Remove an epub from the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a rm -d 'Remove an epub from the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a delete -d 'Remove an epub from the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a list -d 'List epubs in the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a ls -d 'List epubs in the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a open -d 'Open an epub from the library'
complete -c epubworm -n '__fish_epubworm_needs_command' -a set-line-length -d 'Set the maximum characters per line'

complete -c epubworm -n '__fish_epubworm_using_command add' -k -a '(__fish_complete_suffix .epub)'
