; Remove only this application's registrations, not the .pdf default value.
DeleteRegValue HKLM "Software\RegisteredApplications" "jPDF Desk"
DeleteRegKey HKLM "Software\jPDF Desk\Capabilities"
DeleteRegKey /ifempty HKLM "Software\jPDF Desk"
DeleteRegValue HKLM "Software\Classes\.pdf\OpenWithProgids" "jPDF.Desk.PDF"
DeleteRegKey /ifempty HKLM "Software\Classes\.pdf\OpenWithProgids"
DeleteRegKey HKLM "Software\Classes\jPDF.Desk.PDF"
DeleteRegKey HKLM "Software\Classes\Applications\jpdf-desk.exe"

System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
