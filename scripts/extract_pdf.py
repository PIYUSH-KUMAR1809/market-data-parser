import PyPDF2

def extract_text_from_pdf(pdf_path, txt_path):
    with open(pdf_path, 'rb') as file:
        reader = PyPDF2.PdfReader(file)
        text = ""
        for page_num in range(len(reader.pages)):
            page = reader.pages[page_num]
            text += page.extract_text() + "\n"
    
    with open(txt_path, 'w', encoding='utf-8') as f:
        f.write(text)

if __name__ == "__main__":
    extract_text_from_pdf("../data/l2_data/Real_timeFOL1L2V18_20260105110858.pdf", "../data/l2_data/protocol.txt")
