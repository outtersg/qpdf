#include <qpdf/QPDFJob.hh>

// See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <cstdio>
#include <ctype.h>
#include <memory>
#include <sstream>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/JSONHandler.hh>

#include <qpdf/auto_job_schema.hh>
static JSON JOB_SCHEMA = JSON::parse(JOB_SCHEMA_DATA);

namespace
{
    class ArgParser
    {
      public:
        ArgParser(QPDFArgParser& ap,
                  std::shared_ptr<QPDFJob::Config> c_main, QPDFJob& o);
        void parseOptions();

      private:
#       include <qpdf/auto_job_decl.hh>

        void usage(std::string const& message);
        void initOptionTables();
        void doFinalChecks();
        void parseUnderOverlayOptions(QPDFJob::UnderOverlay*);
        void parseRotationParameter(std::string const&);
        std::vector<int> parseNumrange(char const* range, int max,
                                       bool throw_error = false);

        QPDFArgParser ap;
        QPDFJob& o;
        std::shared_ptr<QPDFJob::Config> c_main;
        std::shared_ptr<QPDFJob::CopyAttConfig> c_copy_att;
        std::shared_ptr<QPDFJob::AttConfig> c_att;
        std::vector<char*> accumulated_args; // points to member in ap
        char* pages_password;
    };
}

ArgParser::ArgParser(QPDFArgParser& ap,
                     std::shared_ptr<QPDFJob::Config> c_main, QPDFJob& o) :
    ap(ap),
    o(o),
    c_main(c_main),
    pages_password(nullptr)
{
    initOptionTables();
}

#include <qpdf/auto_job_help.hh>

void
ArgParser::initOptionTables()
{

#   include <qpdf/auto_job_init.hh>
    this->ap.addFinalCheck(
        QPDFArgParser::bindBare(&ArgParser::doFinalChecks, this));
    // add_help is defined in auto_job_help.hh
    add_help(this->ap);
}

void
ArgParser::argPositional(char* arg)
{
    if (o.infilename == 0)
    {
        o.infilename = QUtil::make_shared_cstr(arg);
    }
    else if (o.outfilename == 0)
    {
        o.outfilename = QUtil::make_shared_cstr(arg);
    }
    else
    {
        usage(std::string("unknown argument ") + arg);
    }
}

void
ArgParser::argVersion()
{
    auto whoami = this->ap.getProgname();
    std::cout
        << whoami << " version " << QPDF::QPDFVersion() << std::endl
        << "Run " << whoami << " --copyright to see copyright and license information."
        << std::endl;
}

void
ArgParser::argCopyright()
{
    // Make sure the output looks right on an 80-column display.
    //               1         2         3         4         5         6         7         8
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    std::cout
        << this->ap.getProgname()
        << " version " << QPDF::QPDFVersion() << std::endl
        << std::endl
        << "Copyright (c) 2005-2021 Jay Berkenbilt"
        << std::endl
        << "QPDF is licensed under the Apache License, Version 2.0 (the \"License\");"
        << std::endl
        << "you may not use this file except in compliance with the License."
        << std::endl
        << "You may obtain a copy of the License at"
        << std::endl
        << std::endl
        << "  http://www.apache.org/licenses/LICENSE-2.0"
        << std::endl
        << std::endl
        << "Unless required by applicable law or agreed to in writing, software"
        << std::endl
        << "distributed under the License is distributed on an \"AS IS\" BASIS,"
        << std::endl
        << "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
        << std::endl
        << "See the License for the specific language governing permissions and"
        << std::endl
        << "limitations under the License."
        << std::endl
        << std::endl
        << "Versions of qpdf prior to version 7 were released under the terms"
        << std::endl
        << "of version 2.0 of the Artistic License. At your option, you may"
        << std::endl
        << "continue to consider qpdf to be licensed under those terms. Please"
        << std::endl
        << "see the manual for additional information."
        << std::endl;
}

void
ArgParser::argJsonHelp()
{
    // Make sure the output looks right on an 80-column display.
    //               1         2         3         4         5         6         7         8
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    std::cout
        << "The json block below contains the same structure with the same keys as the"
        << std::endl
        << "json generated by qpdf. In the block below, the values are descriptions of"
        << std::endl
        << "the meanings of those entries. The specific contract guaranteed by qpdf in"
        << std::endl
        << "its json representation is explained in more detail in the manual. You can"
        << std::endl
        << "specify a subset of top-level keys when you invoke qpdf, but the \"version\""
        << std::endl
        << "and \"parameters\" keys will always be present. Note that the \"encrypt\""
        << std::endl
        << "key's values will be populated for non-encrypted files. Some values will"
        << std::endl
        << "be null, and others will have values that apply to unencrypted files."
        << std::endl
        << QPDFJob::json_schema().unparse()
        << std::endl;
}

void
ArgParser::argShowCrypto()
{
    auto crypto = QPDFCryptoProvider::getRegisteredImpls();
    std::string default_crypto = QPDFCryptoProvider::getDefaultProvider();
    std::cout << default_crypto << std::endl;
    for (auto const& iter: crypto)
    {
        if (iter != default_crypto)
        {
            std::cout << iter << std::endl;
        }
    }
}

void
ArgParser::argPasswordFile(char* parameter)
{
    std::list<std::string> lines;
    if (strcmp(parameter, "-") == 0)
    {
        QTC::TC("qpdf", "qpdf password stdin");
        lines = QUtil::read_lines_from_file(std::cin);
    }
    else
    {
        QTC::TC("qpdf", "qpdf password file");
        lines = QUtil::read_lines_from_file(parameter);
    }
    if (lines.size() >= 1)
    {
        o.password = QUtil::make_shared_cstr(lines.front());

        if (lines.size() > 1)
        {
            std::cerr << this->ap.getProgname()
                      << ": WARNING: all but the first line of"
                      << " the password file are ignored" << std::endl;
        }
    }
}

void
ArgParser::argEncrypt()
{
    this->accumulated_args.clear();
    if (this->ap.isCompleting() && this->ap.argsLeft() == 0)
    {
        this->ap.insertCompletion("user-password");
    }
    this->ap.selectOptionTable(O_ENCRYPTION);
}

void
ArgParser::argEncPositional(char* arg)
{
    this->accumulated_args.push_back(arg);
    size_t n_args = this->accumulated_args.size();
    if (n_args < 3)
    {
        if (this->ap.isCompleting() && (this->ap.argsLeft() == 0))
        {
            if (n_args == 1)
            {
                this->ap.insertCompletion("owner-password");
            }
            else if (n_args == 2)
            {
                this->ap.insertCompletion("40");
                this->ap.insertCompletion("128");
                this->ap.insertCompletion("256");
            }
        }
        return;
    }
    o.user_password = this->accumulated_args.at(0);
    o.owner_password = this->accumulated_args.at(1);
    std::string len_str = this->accumulated_args.at(2);
    if (len_str == "40")
    {
        o.keylen = 40;
        this->ap.selectOptionTable(O_40_BIT_ENCRYPTION);
    }
    else if (len_str == "128")
    {
        o.keylen = 128;
        this->ap.selectOptionTable(O_128_BIT_ENCRYPTION);
    }
    else if (len_str == "256")
    {
        o.keylen = 256;
        o.use_aes = true;
        this->ap.selectOptionTable(O_256_BIT_ENCRYPTION);
    }
    else
    {
        usage("encryption key length must be 40, 128, or 256");
    }
}

void
ArgParser::argPasswordMode(char* parameter)
{
    if (strcmp(parameter, "bytes") == 0)
    {
        o.password_mode = QPDFJob::pm_bytes;
    }
    else if (strcmp(parameter, "hex-bytes") == 0)
    {
        o.password_mode = QPDFJob::pm_hex_bytes;
    }
    else if (strcmp(parameter, "unicode") == 0)
    {
        o.password_mode = QPDFJob::pm_unicode;
    }
    else if (strcmp(parameter, "auto") == 0)
    {
        o.password_mode = QPDFJob::pm_auto;
    }
    else
    {
        usage("invalid password-mode option");
    }
}

void
ArgParser::argEnc256AllowInsecure()
{
    o.allow_insecure = true;
}

void
ArgParser::argPages()
{
    if (! o.page_specs.empty())
    {
        usage("the --pages may only be specified one time");
    }
    this->accumulated_args.clear();
    this->ap.selectOptionTable(O_PAGES);
}

void
ArgParser::argPagesPassword(char* parameter)
{
    if (this->pages_password != nullptr)
    {
        QTC::TC("qpdf", "qpdf duplicated pages password");
        usage("--password already specified for this file");
    }
    if (this->accumulated_args.size() != 1)
    {
        QTC::TC("qpdf", "qpdf misplaced pages password");
        usage("in --pages, --password must immediately follow a file name");
    }
    this->pages_password = parameter;
}

void
ArgParser::argPagesPositional(char* arg)
{
    if (arg == nullptr)
    {
        if (this->accumulated_args.empty())
        {
            return;
        }
    }
    else
    {
        this->accumulated_args.push_back(arg);
    }

    char const* file = this->accumulated_args.at(0);
    char const* range = nullptr;

    size_t n_args = this->accumulated_args.size();
    if (n_args >= 2)
    {
        range = this->accumulated_args.at(1);
    }

    // See if the user omitted the range entirely, in which case we
    // assume "1-z".
    char* next_file = nullptr;
    if (range == nullptr)
    {
        if (arg == nullptr)
        {
            // The filename or password was the last argument
            QTC::TC("qpdf", "qpdf pages range omitted at end",
                    this->pages_password == nullptr ? 0 : 1);
        }
        else
        {
            // We need to accumulate some more arguments
            return;
        }
    }
    else
    {
        try
        {
            parseNumrange(range, 0, true);
        }
        catch (std::runtime_error& e1)
        {
            // The range is invalid.  Let's see if it's a file.
            if (strcmp(range, ".") == 0)
            {
                // "." means the input file.
                QTC::TC("qpdf", "qpdf pages range omitted with .");
            }
            else if (QUtil::file_can_be_opened(range))
            {
                QTC::TC("qpdf", "qpdf pages range omitted in middle");
                // Yup, it's a file.
            }
            else
            {
                // Give the range error
                usage(e1.what());
            }
            next_file = const_cast<char*>(range);
            range = nullptr;
        }
    }
    if (range == nullptr)
    {
        range = "1-z";
    }
    o.page_specs.push_back(QPDFJob::PageSpec(file, this->pages_password, range));
    this->accumulated_args.clear();
    this->pages_password = nullptr;
    if (next_file != nullptr)
    {
        this->accumulated_args.push_back(next_file);
    }
}

void
ArgParser::argEndPages()
{
    argPagesPositional(nullptr);
    if (o.page_specs.empty())
    {
        usage("--pages: no page specifications given");
    }
}

void
ArgParser::argUnderlay()
{
    parseUnderOverlayOptions(&o.underlay);
}

void
ArgParser::argOverlay()
{
    parseUnderOverlayOptions(&o.overlay);
}

void
ArgParser::argRotate(char* parameter)
{
    parseRotationParameter(parameter);
}

void
ArgParser::argAddAttachment()
{
    this->c_att = c_main->addAttachment();
    this->ap.selectOptionTable(O_ATTACHMENT);
}

void
ArgParser::argCopyAttachmentsFrom()
{
    this->c_copy_att = c_main->copyAttachmentsFrom();
    this->ap.selectOptionTable(O_COPY_ATTACHMENT);
}

void
ArgParser::argStreamData(char* parameter)
{
    o.stream_data_set = true;
    if (strcmp(parameter, "compress") == 0)
    {
        o.stream_data_mode = qpdf_s_compress;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.stream_data_mode = qpdf_s_preserve;
    }
    else if (strcmp(parameter, "uncompress") == 0)
    {
        o.stream_data_mode = qpdf_s_uncompress;
    }
    else
    {
        // If this happens, it means streamDataChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid stream-data option");
    }
}

void
ArgParser::argDecodeLevel(char* parameter)
{
    o.decode_level_set = true;
    if (strcmp(parameter, "none") == 0)
    {
        o.decode_level = qpdf_dl_none;
    }
    else if (strcmp(parameter, "generalized") == 0)
    {
        o.decode_level = qpdf_dl_generalized;
    }
    else if (strcmp(parameter, "specialized") == 0)
    {
        o.decode_level = qpdf_dl_specialized;
    }
    else if (strcmp(parameter, "all") == 0)
    {
        o.decode_level = qpdf_dl_all;
    }
    else
    {
        // If this happens, it means decodeLevelChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid option");
    }
}

void
ArgParser::argObjectStreams(char* parameter)
{
    o.object_stream_set = true;
    if (strcmp(parameter, "disable") == 0)
    {
        o.object_stream_mode = qpdf_o_disable;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.object_stream_mode = qpdf_o_preserve;
    }
    else if (strcmp(parameter, "generate") == 0)
    {
        o.object_stream_mode = qpdf_o_generate;
    }
    else
    {
        // If this happens, it means objectStreamsChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid object stream mode");
    }
}

void
ArgParser::argRemoveUnreferencedResources(char* parameter)
{
    if (strcmp(parameter, "auto") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_auto;
    }
    else if (strcmp(parameter, "yes") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_yes;
    }
    else if (strcmp(parameter, "no") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_no;
    }
    else
    {
        // If this happens, it means remove_unref_choices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid value for --remove-unreferenced-page-resources");
    }
}

void
ArgParser::argShowObject(char* parameter)
{
    QPDFJob::parse_object_id(parameter, o.show_trailer, o.show_obj, o.show_gen);
    o.require_outfile = false;
}

void
ArgParser::argEnc40Print(char* parameter)
{
    o.r2_print = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc40Modify(char* parameter)
{
    o.r2_modify = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc40Extract(char* parameter)
{
    o.r2_extract = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc40Annotate(char* parameter)
{
    o.r2_annotate = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Accessibility(char* parameter)
{
    o.r3_accessibility = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Extract(char* parameter)
{
    o.r3_extract = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Print(char* parameter)
{
    if (strcmp(parameter, "full") == 0)
    {
        o.r3_print = qpdf_r3p_full;
    }
    else if (strcmp(parameter, "low") == 0)
    {
        o.r3_print = qpdf_r3p_low;
    }
    else if (strcmp(parameter, "none") == 0)
    {
        o.r3_print = qpdf_r3p_none;
    }
    else
    {
        usage("invalid print option");
    }
}

void
ArgParser::argEnc128Modify(char* parameter)
{
    if (strcmp(parameter, "all") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = true;
        o.r3_form_filling = true;
        o.r3_modify_other = true;
    }
    else if (strcmp(parameter, "annotate") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = true;
        o.r3_form_filling = true;
        o.r3_modify_other = false;
    }
    else if (strcmp(parameter, "form") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = false;
        o.r3_form_filling = true;
        o.r3_modify_other = false;
    }
    else if (strcmp(parameter, "assembly") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = false;
        o.r3_form_filling = false;
        o.r3_modify_other = false;
    }
    else if (strcmp(parameter, "none") == 0)
    {
        o.r3_assemble = false;
        o.r3_annotate_and_form = false;
        o.r3_form_filling = false;
        o.r3_modify_other = false;
    }
    else
    {
        usage("invalid modify option");
    }
}

void
ArgParser::argEnc128CleartextMetadata()
{
    o.cleartext_metadata = true;
}

void
ArgParser::argEnc128Assemble(char* parameter)
{
    o.r3_assemble = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Annotate(char* parameter)
{
    o.r3_annotate_and_form = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Form(char* parameter)
{
    o.r3_form_filling = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128ModifyOther(char* parameter)
{
    o.r3_modify_other = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128UseAes(char* parameter)
{
    o.use_aes = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128ForceV4()
{
    o.force_V4 = true;
}

void
ArgParser::argEnc256ForceR5()
{
    o.force_R5 = true;
}

void
ArgParser::argEndEncryption()
{
    o.encrypt = true;
    o.decrypt = false;
    o.copy_encryption = false;
}

void
ArgParser::argEnd40BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argEnd128BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argEnd256BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argUOPositional(char* arg)
{
    if (! o.under_overlay->filename.empty())
    {
        usage(o.under_overlay->which + " file already specified");
    }
    else
    {
        o.under_overlay->filename = arg;
    }
}

void
ArgParser::argUOTo(char* parameter)
{
    parseNumrange(parameter, 0);
    o.under_overlay->to_nr = parameter;
}

void
ArgParser::argUOFrom(char* parameter)
{
    if (strlen(parameter))
    {
        parseNumrange(parameter, 0);
    }
    o.under_overlay->from_nr = parameter;
}

void
ArgParser::argUORepeat(char* parameter)
{
    if (strlen(parameter))
    {
        parseNumrange(parameter, 0);
    }
    o.under_overlay->repeat_nr = parameter;
}

void
ArgParser::argUOPassword(char* parameter)
{
    o.under_overlay->password = QUtil::make_shared_cstr(parameter);
}

void
ArgParser::argEndUnderlayOverlay()
{
    if (o.under_overlay->filename.empty())
    {
        usage(o.under_overlay->which + " file not specified");
    }
    o.under_overlay = 0;
}

void
ArgParser::argAttPositional(char* arg)
{
    c_att->path(arg);
}

void
ArgParser::argEndAttachment()
{
    c_att->end();
    c_att = nullptr;
}

void
ArgParser::argCopyAttPositional(char* arg)
{
    c_copy_att->path(arg);
}

void
ArgParser::argEndCopyAttachment()
{
    c_copy_att->end();
    c_copy_att = nullptr;
}

void
ArgParser::argJobJsonFile(char* parameter)
{
    PointerHolder<char> file_buf;
    size_t size;
    QUtil::read_file_into_memory(parameter, file_buf, size);
    try
    {
        o.initializeFromJson(std::string(file_buf.getPointer(), size));
    }
    catch (std::exception& e)
    {
        throw std::runtime_error(
            "error with job-json file " + std::string(parameter) + " " +
            e.what() + "\nRun " + this->ap.getProgname() +
            "--job-json-help for information on the file format.");
    }
}

void
ArgParser::argJobJsonHelp()
{
    std::cout << JOB_SCHEMA_DATA << std::endl;
}

void
ArgParser::usage(std::string const& message)
{
    this->ap.usage(message);
}

std::vector<int>
ArgParser::parseNumrange(char const* range, int max, bool throw_error)
{
    try
    {
        return QUtil::parse_numrange(range, max);
    }
    catch (std::runtime_error& e)
    {
        if (throw_error)
        {
            throw(e);
        }
        else
        {
            usage(e.what());
        }
    }
    return std::vector<int>();
}

void
ArgParser::parseUnderOverlayOptions(QPDFJob::UnderOverlay* uo)
{
    o.under_overlay = uo;
    this->ap.selectOptionTable(O_UNDERLAY_OVERLAY);
}

void
ArgParser::parseRotationParameter(std::string const& parameter)
{
    std::string angle_str;
    std::string range;
    size_t colon = parameter.find(':');
    int relative = 0;
    if (colon != std::string::npos)
    {
        if (colon > 0)
        {
            angle_str = parameter.substr(0, colon);
        }
        if (colon + 1 < parameter.length())
        {
            range = parameter.substr(colon + 1);
        }
    }
    else
    {
        angle_str = parameter;
    }
    if (angle_str.length() > 0)
    {
        char first = angle_str.at(0);
        if ((first == '+') || (first == '-'))
        {
            relative = ((first == '+') ? 1 : -1);
            angle_str = angle_str.substr(1);
        }
        else if (! QUtil::is_digit(angle_str.at(0)))
        {
            angle_str = "";
        }
    }
    if (range.empty())
    {
        range = "1-z";
    }
    bool range_valid = false;
    try
    {
        parseNumrange(range.c_str(), 0, true);
        range_valid = true;
    }
    catch (std::runtime_error const&)
    {
        // ignore
    }
    if (range_valid &&
        ((angle_str == "0") ||(angle_str == "90") ||
         (angle_str == "180") || (angle_str == "270")))
    {
        int angle = QUtil::string_to_int(angle_str.c_str());
        if (relative == -1)
        {
            angle = -angle;
        }
        o.rotations[range] = QPDFJob::RotationSpec(angle, (relative != 0));
    }
    else
    {
        usage("invalid parameter to rotate: " + parameter);
    }
}

void
ArgParser::parseOptions()
{
    try
    {
        this->ap.parseArgs();
    }
    catch (QPDFArgParser::Usage& e)
    {
        usage(e.what());
    }
}

void
ArgParser::doFinalChecks()
{
    try
    {
        o.checkConfiguration();
    }
    catch (std::runtime_error& e)
    {
        usage(e.what());
    }
}

void
QPDFJob::initializeFromArgv(int argc, char* argv[], char const* progname_env)
{
    if (progname_env == nullptr)
    {
        progname_env = "QPDF_EXECUTABLE";
    }
    QPDFArgParser qap(argc, argv, progname_env);
    setMessagePrefix(qap.getProgname());
    ArgParser ap(qap, config(), *this);
    ap.parseOptions();
}

void
QPDFJob::initializeFromJson(std::string const& json)
{
    std::list<std::string> errors;
    JSON j = JSON::parse(json);
    if (! j.checkSchema(JOB_SCHEMA, JSON::f_optional, errors))
    {
        std::ostringstream msg;
        msg << this->m->message_prefix
            << ": job json has errors:";
        for (auto const& error: errors)
        {
            msg << std::endl << "  " << error;
        }
        throw std::runtime_error(msg.str());
    }

    JSONHandler jh;
    {
        jh.addDictHandlers(
            [](std::string const&){},
            [](std::string const&){});

        auto input = std::make_shared<JSONHandler>();
        auto input_file = std::make_shared<JSONHandler>();
        auto input_file_name = std::make_shared<JSONHandler>();
        auto output = std::make_shared<JSONHandler>();
        auto output_file = std::make_shared<JSONHandler>();
        auto output_file_name = std::make_shared<JSONHandler>();
        auto output_options = std::make_shared<JSONHandler>();
        auto output_options_qdf = std::make_shared<JSONHandler>();

        input->addDictHandlers(
            [](std::string const&){},
            [](std::string const&){});
        input_file->addDictHandlers(
            [](std::string const&){},
            [](std::string const&){});
        output->addDictHandlers(
            [](std::string const&){},
            [](std::string const&){});
        output_file->addDictHandlers(
            [](std::string const&){},
            [](std::string const&){});
        output_options->addDictHandlers(
            [](std::string const&){},
            [](std::string const&){});

        jh.addDictKeyHandler("input", input);
        input->addDictKeyHandler("file", input_file);
        input_file->addDictKeyHandler("name", input_file_name);
        jh.addDictKeyHandler("output", output);
        output->addDictKeyHandler("file", output_file);
        output_file->addDictKeyHandler("name", output_file_name);
        output->addDictKeyHandler("options", output_options);
        output_options->addDictKeyHandler("qdf", output_options_qdf);

        input_file_name->addStringHandler(
            [this](std::string const&, std::string const& v) {
                this->infilename = QUtil::make_shared_cstr(v);
            });
        output_file_name->addStringHandler(
            [this](std::string const&, std::string const& v) {
                this->outfilename = QUtil::make_shared_cstr(v);
            });
        output_options_qdf->addBoolHandler(
            [this](std::string const&, bool v) {
                this->qdf_mode = v;
            });
    }

    // {
    //   "input": {
    //     "file": {
    //       "name": "/home/ejb/source/examples/pdf/minimal.pdf"
    //     }
    //   },
    //   "output": {
    //     "file": {
    //       "name": "/tmp/a.pdf"
    //     },
    //     "options": {
    //       "qdf": true
    //     }
    //   }
    // }

    jh.handle(".", j);
}
