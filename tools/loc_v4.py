import json, glob, os
D = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Resources\Localization"
T = {'en': 'Habilitation: {0}', 'fr': 'Habilitation : {0}', 'de': 'Befugnis: {0}', 'es': 'Habilitación: {0}', 'it': 'Abilitazione: {0}',
     'pt': 'Habilitação: {0}', 'pl': 'Uprawnienie: {0}', 'ru': 'Допуск: {0}', 'uk': 'Допуск: {0}', 'cs': 'Oprávnění: {0}',
     'hu': 'Jogosultság: {0}', 'tr': 'Yetki: {0}', 'ja': '資格：{0}', 'ko': '자격: {0}', 'zh': '资质：{0}'}
for f in glob.glob(os.path.join(D, '*.json')):
    lang = os.path.splitext(os.path.basename(f))[0]
    d = json.load(open(f, encoding='utf-8'))
    d['mam.reward'] = T[lang]
    open(f, 'w', encoding='utf-8').write(json.dumps(d, ensure_ascii=False, indent=2) + '\n')
print('ok')
