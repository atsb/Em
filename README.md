# Migration to CodeBerg

Em has migrated to EU-based CodeBerg! https://codeberg.org/atsb/Em

No further developments will be done here and the repo will be closed on GitHub in 6 months.

# EM
This is the Editor for Mortals 'em' by George Coulouris.  An ancestor of 'ex/vi'.

# INFO
This contains the original fixed em code.

# LICENSING
Em never really had a licence, it was given to anyone who asked.  8 years ago I clarified this licence with George himself and he was happy for it to be given to anyone and to be used for any purpose.
Since 'ed' which it is based on, is now under the MIT licence from the original author (Ken) due to Plan 9, I believe also putting George's Em code under the same licence fulfills his wish for his software.

Sadly, George passed in November 2024.  It was a pleasure to work with him for a few short months on this, working with a UNIX legend was an amazing experience.

I will add an email correspondence from George where he clarified this licence for evidence purposes only.  To back up the licencing terms.  This version does contain code from CUNY and other univerisities with their additions, which was also within the UNIX source dumps of many systems I had.  Since they gave their changes away on the same terms, I believe this is also acceptable.  The contributions are quite small.

<img width="1186" height="306" alt="Screenshot 2025-08-13 112341" src="https://github.com/user-attachments/assets/6ee4079a-77d8-404c-87e0-0ff90252b145" />

# CURRENT
Currently I have merged the useful parts of CUNY and KENT as well as one part of AUASM (as well as reimplementing standard UNIX signals from illumos) into master.
Other parts were mainly useful for timesharing (allowing the editing of files not owned by you, or setting chmod on files that were changed).
Since we don't do that anymore, it's pointless including it.

EM now runs on any curses compatible system, even Windows :)

<img width="1130" height="642" alt="Screenshot 2025-08-13 022812" src="https://github.com/user-attachments/assets/e0b3acb8-7db7-4ab7-9d1e-de7d71cf0cb8" />
<img width="830" height="667" alt="Screenshot 2025-08-13 030730" src="https://github.com/user-attachments/assets/3ab95776-21ca-47f7-a1b2-1ec1746ee6e0" />
