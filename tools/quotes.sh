# Prints a random quote in green. Source from a repo's .envrc.
quotes=(
  "Ego is a structure that is erected by a neurotic individual who is a member of a neurotic culture against the facts of the matter. And culture, which we put on like an overcoat, is the collectivized consensus about what sort of neurotic behaviors are acceptable. --Terrance McKenna"
  "It's all about love... making someone elses existence just a little easier. Nothing else matters, I know this now --Terence Mckenna"
  "i was ashamed of myself when i realized life was a costume party, and i attended with my real face - franz kafka"
  "The sensitive suffer more; but they love more, and dream more -Augusto Cury"
  "Everything should be made as simple as possible, but not simpler - Einstein"
  "The world, Chico, and everything in it. - Scarface"
  "hazlo, y si te da miedo, hazlo con miedo... - Andrea Adrich"
)
echo -e "\e[32m${quotes[RANDOM % ${#quotes[@]}]}\e[0m"
